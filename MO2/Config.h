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
	uint64_t maxOverallMem;  // total memory in bytes; power of 2
	uint64_t memPerFrame;    // bytes per frame, which is also bytes per page; power of 2
	uint64_t minMemPerProc;  // bytes a scheduler-start process may need; power of 2 in [2^6, 2^16]
	uint64_t maxMemPerProc;
};

// Every process's memory, whether from screen -s or scheduler-start, must be a power of 2 in this range
const uint64_t MIN_PROC_MEM = 64;     // 2^6
const uint64_t MAX_PROC_MEM = 65536;  // 2^16

// A power of 2 has exactly one bit set; n - 1 clears it and sets every bit below it
inline bool isPowerOfTwo(uint64_t n) {
	return n != 0 && (n & (n - 1)) == 0;
}
