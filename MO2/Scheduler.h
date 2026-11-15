#pragma once
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include "Config.h"
#include "Process.h"
#include "ProcessManager.h"
using namespace std;

// Real-time CPU scheduler. One background thread counts CPU cycles; every cycle each
// core runs at most one instruction of the process it holds. Supports first come first served (fcfs) and round robin (rr). The scheduler is responsible for:
//
// Per cycle: fill idle cores from the ready queue -> wake sleepers -> run every core
//            -> cpuCycles++ -> (if generating) create a dummy process every
//            batch-process-freq cycles.
//
// Memory: a process holds its memory from creation until it terminates, whether it is
// running, ready or sleeping. Memory is reserved before a process is created, so one
// that doesn't fit is never created.
class Scheduler {
private:
	// How long one CPU cycle lasts in real time. Only changes how fast things look,
	// not the order in which they happen. Lower = faster emulator.
	static constexpr int TICK_MS = 50;

	struct Core {
		shared_ptr<Process> process;  // nullptr when idle
		uint64_t quantumLeft = 0;     // cycles left in this time slice (rr only)
		uint64_t delayLeft = 0;       // busy-wait cycles before the next instruction
	};

	Config config;
	ProcessManager& manager;

	mutex mtx;                              // guards everything below
	vector<Core> cores;
	deque<shared_ptr<Process>> readyQueue;
	vector<shared_ptr<Process>> sleeping;   // gave up the CPU with SLEEP
	vector<shared_ptr<Process>> finished;   // in order of completion
	uint64_t cpuCycles;
	uint64_t memoryUsed;                    // bytes held by every process that hasn't terminated
	uint64_t idleTicks;                     // one per empty core per cycle
	uint64_t activeTicks;                   // one per occupied core per cycle, busy-waiting included

	thread worker;
	atomic<bool> alive;
	atomic<bool> generating;
	atomic<bool> memoryLimitHit;            // set when generation stopped for lack of memory

	bool isRoundRobin() const {
		return config.scheduler == "rr";
	}

	bool fits(uint64_t mem) const {
		return memoryUsed + mem <= config.maxOverallMem;
	}

	// Every way into `finished` goes through here, so each process's memory is freed exactly once
	void finish(shared_ptr<Process> p) {
		finished.push_back(p);
		memoryUsed -= p->getMem();
	}

	int coresUsed() const {
		int used = 0;
		for (const Core& core : cores) {
			if (core.process != nullptr) {
				used++;
			}
		}
		return used;
	}

	// SLEEP counts down once per cycle; a process that wakes up rejoins the ready queue
	void wakeSleepers() {
		for (auto it = sleeping.begin(); it != sleeping.end();) {
			(*it)->tickSleep();
			if ((*it)->getState() == READY) {
				readyQueue.push_back(*it);
				it = sleeping.erase(it);
			}
			else {
				++it;
			}
		}
	}

	// Gives every idle core the process at the front of the ready queue
	void dispatch() {
		for (size_t i = 0; i < cores.size(); i++) {
			Core& core = cores[i];
			while (core.process == nullptr && !readyQueue.empty()) {
				shared_ptr<Process> next = readyQueue.front();
				readyQueue.pop_front();

				if (next->isFinished()) {  // e.g. a program with nothing to run
					finish(next);
					continue;
				}
				next->assignCore(int(i));
				core.process = next;
				core.quantumLeft = config.quantumCycles;
				core.delayLeft = 0;  // the first instruction runs right away
			}
		}
	}

	// Takes the process off its core and clears the core
	void vacate(Core& core) {
		core.process->releaseCore();
		core.process = nullptr;
	}

	// One CPU cycle on every busy core
	void runCores() {
		for (Core& core : cores) {
			if (core.process == nullptr) {
				idleTicks++;
				continue;
			}
			activeTicks++;  // holding a process counts as active, even while busy-waiting
			shared_ptr<Process> p = core.process;

			if (core.delayLeft > 0) {
				core.delayLeft--;  // busy-waiting: the process keeps the core but does nothing
			}
			else {
				p->executeNext();
				core.delayLeft = config.delaysPerExec;
			}

			if (p->isFinished()) {
				vacate(core);
				finish(p);
			}
			else if (p->getState() == WAITING) {  // executed SLEEP, so give the core up
				vacate(core);
				sleeping.push_back(p);
			}
			else if (isRoundRobin()) {
				core.quantumLeft--;
				if (core.quantumLeft == 0) {
					if (readyQueue.empty()) {
						core.quantumLeft = config.quantumCycles;  // nobody waiting, keep going
					}
					else {
						vacate(core);
						readyQueue.push_back(p);
					}
				}
			}
		}
	}

	void tick() {
		lock_guard<mutex> lock(mtx);

		// Dispatch before waking: a process that wakes this cycle only gets a core next
		// cycle, so SLEEP(X) sits out exactly X cycles.
		dispatch();
		wakeSleepers();
		runCores();
		cpuCycles++;

		if (generating && cpuCycles % config.batchProcessFreq == 0) {
			uint64_t mem = manager.rollMemory();
			if (fits(mem)) {
				memoryUsed += mem;
				readyQueue.push_back(manager.createAutoProcess(mem));
			}
			else {
				// Temporary until paging: stop generating instead of waiting for memory.
				// The main menu prints the message, since this thread must not write to the console.
				generating = false;
				memoryLimitHit = true;
			}
		}
	}

	void run() {
		while (alive) {
			tick();
			this_thread::sleep_for(chrono::milliseconds(TICK_MS));
		}
	}

	// One line for a process: name, creation time, then core and progress or "Finished"
	static void printProcessLine(stringstream& out, const Process& p, const string& where) {
		out << left << setw(12) << p.getName()
			<< setw(26) << ("(" + p.getCreatedAt() + ")")
			<< setw(14) << where
			<< p.getCtr() << " / " << p.getTotalInstructions() << "\n";
	}

public:
	Scheduler(const Config& param_config, ProcessManager& param_manager)
		: config(param_config), manager(param_manager), cores(param_config.numCpu),
		  cpuCycles(0), memoryUsed(0), idleTicks(0), activeTicks(0),
		  alive(false), generating(false), memoryLimitHit(false) {
	}

	~Scheduler() {
		stop();
	}

	Scheduler(const Scheduler&) = delete;
	Scheduler& operator=(const Scheduler&) = delete;

	// Starts the CPU thread. It keeps running until stop() or destruction.
	void start() {
		if (alive.exchange(true)) {
			return;
		}
		worker = thread(&Scheduler::run, this);
	}

	void stop() {
		alive = false;
		if (worker.joinable()) {
			worker.join();
		}
	}

	// screen -s: takes `mem` bytes before the process is created.
	// Returns false, taking nothing, if there isn't enough memory left.
	bool reserve(uint64_t mem) {
		lock_guard<mutex> lock(mtx);
		if (!fits(mem)) {
			return false;
		}
		memoryUsed += mem;
		return true;
	}

	// screen -s: gives back a reservation whose process was never created (name already taken)
	void release(uint64_t mem) {
		lock_guard<mutex> lock(mtx);
		memoryUsed -= mem;
	}

	// Puts a process in the ready queue (used by screen -s, after reserve)
	void enqueue(shared_ptr<Process> process) {
		lock_guard<mutex> lock(mtx);
		if (process->isFinished()) {
			finish(process);
		}
		else {
			readyQueue.push_back(process);
		}
	}

	// True once after generation stopped for lack of memory; reading it clears it
	bool takeMemoryLimitHit() {
		return memoryLimitHit.exchange(false);
	}

	// scheduler-start / scheduler-stop. Return false if already in that state.
	bool startGenerating() {
		return !generating.exchange(true);
	}

	bool stopGenerating() {
		return generating.exchange(false);
	}

	// Text for screen -ls and report-util: utilization, then running and finished processes
	string buildReport() {
		lock_guard<mutex> lock(mtx);  // the CPU thread is paused while we read

		int used = coresUsed();
		int total = int(cores.size());

		stringstream out;
		out << "CPU utilization: " << (used * 100 / total) << "%\n";
		out << "Cores used: " << used << "\n";
		out << "Cores available: " << (total - used) << "\n\n";
		out << "--------------------------------------\n";

		out << "Running processes:\n";
		for (size_t i = 0; i < cores.size(); i++) {
			if (cores[i].process != nullptr) {
				printProcessLine(out, *cores[i].process, "Core: " + to_string(i));
			}
		}

		out << "\nFinished processes:\n";
		for (const shared_ptr<Process>& p : finished) {
			printProcessLine(out, *p, "Finished");
		}
		out << "--------------------------------------\n";
		return out.str();
	}

	// Text for process-smi on the main menu: CPU and memory use, then each running process's memory
	string buildMemoryReport() {
		lock_guard<mutex> lock(mtx);  // the CPU thread is paused while we read

		stringstream out;
		out << "CPU utilization: " << (coresUsed() * 100 / int(cores.size())) << "%\n";
		out << "Memory Usage: " << memoryUsed << " B / " << config.maxOverallMem << " B\n";
		out << "Memory Util: " << (memoryUsed * 100 / config.maxOverallMem) << "%\n\n";
		out << "--------------------------------------\n";

		out << "Running processes and memory usage:\n";
		for (const Core& core : cores) {
			if (core.process != nullptr) {
				out << left << setw(12) << core.process->getName() << core.process->getMem() << " B\n";
			}
		}
		out << "--------------------------------------\n";
		return out.str();
	}

	// Text for vmstat. Ticks are counted per core, so total = cpu cycles * num-cpu = idle + active.
	string buildVmstat() {
		lock_guard<mutex> lock(mtx);  // the CPU thread is paused while we read

		stringstream out;
		out << "Used memory: " << memoryUsed << " B\n";
		out << "Free memory: " << (config.maxOverallMem - memoryUsed) << " B\n";
		out << "Idle cpu ticks: " << idleTicks << "\n";
		out << "Active cpu ticks: " << activeTicks << "\n";
		out << "Total cpu ticks: " << (idleTicks + activeTicks) << "\n";
		out << "Num paged in: TBA\n";   // until paging is implemented
		out << "Num paged out: TBA\n";
		return out.str();
	}
};