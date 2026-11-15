#pragma once
#include <string>
#include <cstdint>
using namespace std;

struct Config {
	int numCpu;              // [1, 128]
	string scheduler;        // "fcfs" or "rr"
	uint64_t quantumCycles;  // [1, 2^32]
	uint64_t batchProcessFreq;
	uint64_t minIns;
	uint64_t maxIns;
	uint64_t delaysPerExec;  // [0, 2^32]
	uint64_t maxOverallMem;  // total memory in bytes
	uint64_t memPerFrame;    // bytes per frame, which is also bytes per page
	uint64_t minMemPerProc;  // bytes a scheduler-start process may need
	uint64_t maxMemPerProc;
};
