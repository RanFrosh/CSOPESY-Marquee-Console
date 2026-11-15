#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <mutex>
#include <cstdint>
#include <bit>
#include "Config.h"
#include "Process.h"
#include "Generator.h"
using namespace std;

// Keeps every process that has been created, running or finished.
// Used by screen -s / -r / -ls, scheduler-start and report-util.
class ProcessManager {
private:
	Config config;                                               // for the instruction and memory limits
	mt19937_64 rng;                                              // one generator for every program and memory size
	vector<shared_ptr<Process>> processes;                       // in creation order, for listings
	unordered_map<string, shared_ptr<Process>> processesByName;  // for lookups by name
	int nextId;                                                  // shared by screen -s and scheduler-start
	uint64_t nextAutoNumber;                                     // for p01, p02, ... names
	mutex mtx;                                                   // main thread and scheduler thread both use this

	// "p" plus at least two digits: 1 -> p01, 42 -> p42, 1240 -> p1240
	static string formatAutoName(uint64_t number) {
		return "p" + string(number < 10 ? "0" : "") + to_string(number);
	}

	// Creates, numbers and stores a process with a random program and `mem` bytes of memory.
	// The caller must already hold mtx and have checked the name is free.
	shared_ptr<Process> addProcess(const string& name, uint64_t mem) {
		auto process = make_shared<Process>(nextId, name, generateProgram(rng, config.minIns, config.maxIns), mem);
		nextId++;
		processes.push_back(process);
		processesByName[name] = process;
		return process;
	}

public:
	ProcessManager(const Config& param_config) {
		config = param_config;
		rng.seed(random_device{}());
		nextId = 1;
		nextAutoNumber = 1;
	}

	// A random power of 2 from min-mem-per-proc to max-mem-per-proc. Rolled before the
	// process is created, so the scheduler can reject it without creating anything.
	// Both limits are powers of 2 (checked at initialize), so rolling the exponent
	// gives every power of 2 in between the same chance: 256 to 1024 -> 256, 512 or 1024.
	uint64_t rollMemory() {
		lock_guard<mutex> lock(mtx);
		int minExponent = countr_zero(config.minMemPerProc);
		int maxExponent = countr_zero(config.maxMemPerProc);
		return uint64_t(1) << randomBetween(rng, minExponent, maxExponent);
	}

	// For screen -s. Returns nullptr if a process with this name already exists.
	shared_ptr<Process> createProcess(const string& name, uint64_t mem) {
		lock_guard<mutex> lock(mtx);

		if (processesByName.find(name) != processesByName.end()) {
			return nullptr;
		}
		return addProcess(name, mem);
	}

	// For scheduler-start. Uses the next pNN name that isn't taken,
	// skipping any the user already created with screen -s.
	shared_ptr<Process> createAutoProcess(uint64_t mem) {
		lock_guard<mutex> lock(mtx);

		string name;
		do {
			name = formatAutoName(nextAutoNumber);
			nextAutoNumber++;
		} while (processesByName.find(name) != processesByName.end());

		return addProcess(name, mem);
	}

	// Returns the process with this name, finished or not, or nullptr if there isn't one.
	// screen -r also treats a finished process as not found.
	shared_ptr<Process> find(const string& name) {
		lock_guard<mutex> lock(mtx);

		auto found = processesByName.find(name);
		if (found == processesByName.end()) {
			return nullptr;
		}
		return found->second;
	}
};
