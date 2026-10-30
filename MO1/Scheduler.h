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
// Per cycle: wake sleepers -> fill idle cores from the ready queue -> run every core
//            -> cpuCycles++ -> (if generating) create a dummy process every
//            batch-process-freq cycles.
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

	thread worker;
	atomic<bool> alive;
	atomic<bool> generating;

	bool isRoundRobin() const {
		return config.scheduler == "rr";
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
					finished.push_back(next);
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
				continue;
			}
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
				finished.push_back(p);
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

		wakeSleepers();
		dispatch();
		runCores();
		cpuCycles++;

		if (generating && cpuCycles % config.batchProcessFreq == 0) {
			shared_ptr<Process> created = manager.createAutoProcess();
			readyQueue.push_back(created);
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
		  cpuCycles(0), alive(false), generating(false) {
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

	// Puts a process in the ready queue (used by screen -s)
	void enqueue(shared_ptr<Process> process) {
		lock_guard<mutex> lock(mtx);
		if (process->isFinished()) {
			finished.push_back(process);
		}
		else {
			readyQueue.push_back(process);
		}
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

		int used = 0;
		for (const Core& core : cores) {
			if (core.process != nullptr) {
				used++;
			}
		}
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
};