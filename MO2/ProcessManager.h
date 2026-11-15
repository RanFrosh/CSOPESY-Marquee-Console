#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <mutex>
#include <cstdint>
#include "Config.h"
#include "Process.h"
#include "Generator.h"
using namespace std;

// Keeps every process that has been created, running or finished.
// Used by screen -s / -r / -ls, scheduler-start and report-util.
class ProcessManager {
private:
	Config config;                                               // for min-ins and max-ins
	mt19937_64 rng;                                              // one generator for every program
	vector<shared_ptr<Process>> processes;                       // in creation order, for listings
	unordered_map<string, shared_ptr<Process>> processesByName;  // for lookups by name
	int nextId;                                                  // shared by screen -s and scheduler-start
	uint64_t nextAutoNumber;                                     // for p01, p02, ... names
	mutex mtx;                                                   // main thread and scheduler thread both use this

	// "p" plus at least two digits: 1 -> p01, 42 -> p42, 1240 -> p1240
	static string formatAutoName(uint64_t number) {
		return "p" + string(number < 10 ? "0" : "") + to_string(number);
	}

	// Creates, numbers and stores a process with a random program.
	// The caller must already hold mtx and have checked the name is free.
	shared_ptr<Process> addProcess(const string& name) {
		auto process = make_shared<Process>(nextId, name, generateProgram(rng, config.minIns, config.maxIns));
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

	// For screen -s. Returns nullptr if a process with this name already exists.
	shared_ptr<Process> createProcess(const string& name) {
		lock_guard<mutex> lock(mtx);

		if (processesByName.find(name) != processesByName.end()) {
			return nullptr;
		}
		return addProcess(name);
	}

	// For scheduler-start. Uses the next pNN name that isn't taken,
	// skipping any the user already created with screen -s.
	shared_ptr<Process> createAutoProcess() {
		lock_guard<mutex> lock(mtx);

		string name;
		do {
			name = formatAutoName(nextAutoNumber);
			nextAutoNumber++;
		} while (processesByName.find(name) != processesByName.end());

		return addProcess(name);
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
