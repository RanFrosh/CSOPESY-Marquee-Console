#pragma once
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include "Process.h"
using namespace std;

// Random programs for new processes (screen -s and scheduler-start)

const int MAX_FOR_DEPTH = 3;    // how deep FORs can nest
const int MAX_FOR_REPEATS = 5;
const int MAX_FOR_BODY = 5;     // most instructions in one FOR body
const int MAX_SLEEP = 10;
const vector<string> VAR_NAMES = { "x", "y", "z" };

// Random whole number from lo to hi, inclusive
inline uint64_t randomBetween(mt19937_64& rng, uint64_t lo, uint64_t hi) {
	return uniform_int_distribution<uint64_t>(lo, hi)(rng);
}

inline Operand randomVar(mt19937_64& rng) {
	return VAR_NAMES[randomBetween(rng, 0, VAR_NAMES.size() - 1)];
}

// Either a variable name or a literal value, 50/50
inline Operand randomOperand(mt19937_64& rng) {
	if (randomBetween(rng, 0, 1) == 0) {
		return randomVar(rng);
	}
	return uint16_t(randomBetween(rng, 0, 100));
}

// Builds exactly `count` instructions; a FOR counts as 1 plus its body
inline vector<Instruction> generateList(mt19937_64& rng, uint64_t count, int depth) {
	vector<Instruction> list;

	while (count > 0) {
		// A FOR needs room for itself and at least one body instruction
		bool canFor = depth < MAX_FOR_DEPTH && count >= 2;
		auto type = InstructionType(randomBetween(rng, 0, canFor ? 5 : 4));

		switch (type) {
		case InstructionType::PRINT:
			if (randomBetween(rng, 0, 1) == 0) {
				list.push_back({ type, {}, "" });  // "Hello world from <name>"
			}
			else {
				list.push_back({ type, { randomVar(rng) }, "Value from: " });
			}
			count--;
			break;
		case InstructionType::DECLARE:
			list.push_back({ type, { randomVar(rng), uint16_t(randomBetween(rng, 0, 100)) } });
			count--;
			break;
		case InstructionType::ADD:
		case InstructionType::SUBTRACT:
			list.push_back({ type, { randomVar(rng), randomOperand(rng), randomOperand(rng) } });
			count--;
			break;
		case InstructionType::SLEEP:
			list.push_back({ type, { uint16_t(randomBetween(rng, 1, MAX_SLEEP)) } });
			count--;
			break;
		case InstructionType::FOR: {
			uint64_t bodySize = randomBetween(rng, 1, min<uint64_t>(count - 1, MAX_FOR_BODY));
			uint16_t repeats = uint16_t(randomBetween(rng, 1, MAX_FOR_REPEATS));
			list.push_back({ type, {}, "", generateList(rng, bodySize, depth + 1), repeats });
			count -= 1 + bodySize;
			break;
		}
		}
	}
	return list;
}

// A random program with between minIns and maxIns instructions
inline vector<Instruction> generateProgram(mt19937_64& rng, uint64_t minIns, uint64_t maxIns) {
	return generateList(rng, randomBetween(rng, minIns, maxIns), 0);
}
