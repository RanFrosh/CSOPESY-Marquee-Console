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
const int MIN_NAME_LENGTH = 1;
const int MAX_NAME_LENGTH = 8;

// true: every program alternates PRINT("Value from: " + x) and ADD(x, x, 1..10)
// false: random programs from generateList
const bool ALTERNATING_PROGRAMS = true;
const int MIN_ADD_VALUE = 1;
const int MAX_ADD_VALUE = 10;

// Random whole number from lo to hi, inclusive
inline uint64_t randomBetween(mt19937_64& rng, uint64_t lo, uint64_t hi) {
	return uniform_int_distribution<uint64_t>(lo, hi)(rng);
}

// A random name of printable ASCII characters, with no spaces
inline string randomName(mt19937_64& rng) {
	string name;
	uint64_t length = randomBetween(rng, MIN_NAME_LENGTH, MAX_NAME_LENGTH);
	for (uint64_t i = 0; i < length; i++) {
		name += char(randomBetween(rng, '!', '~'));
	}
	return name;
}

// Usually reuses a declared variable; sometimes introduces a new one
inline string pickVariable(mt19937_64& rng, vector<string>& declared) {
	if (declared.empty() || randomBetween(rng, 1, 4) == 1) {
		declared.push_back(randomName(rng));
		return declared.back();
	}
	return declared[randomBetween(rng, 0, declared.size() - 1)];
}

// Either a variable name or a literal value, 50/50
inline Operand randomOperand(mt19937_64& rng, vector<string>& declared) {
	if (randomBetween(rng, 0, 1) == 0) {
		return pickVariable(rng, declared);
	}
	return uint16_t(randomBetween(rng, 0, 100));
}

// Builds exactly `count` instructions; a FOR counts as 1 plus its body.
// `declared` holds every variable the instructions before this point will have
// created. Programs run in the order written and every FOR runs at least once,
// so those variables are guaranteed to exist by the time the next instruction runs.
inline vector<Instruction> generateList(mt19937_64& rng, uint64_t count, int depth, vector<string>& declared) {
	vector<Instruction> list;

	while (count > 0) {
		// A FOR needs room for itself and at least one body instruction
		bool canFor = depth < MAX_FOR_DEPTH && count >= 2;
		auto type = InstructionType(randomBetween(rng, 0, canFor ? 5 : 4));

		switch (type) {
		case InstructionType::PRINT: {
			uint64_t printCase = randomBetween(rng, 0, 2);
			if (printCase == 1) {
				// A random name, which almost certainly doesn't exist
				list.push_back({ type, { randomName(rng) }, "Value from: " });
			}
			else if (printCase == 2 && !declared.empty()) {
				// A variable that is guaranteed to exist
				list.push_back({ type, { declared[randomBetween(rng, 0, declared.size() - 1)] }, "Value from: " });
			}
			else {
				list.push_back({ type, {}, "" });  // "Hello world from <name>"
			}
			count--;
			break;
		}
		case InstructionType::DECLARE:
			list.push_back({ type, { pickVariable(rng, declared), uint16_t(randomBetween(rng, 0, 100)) } });
			count--;
			break;
		case InstructionType::ADD:
		case InstructionType::SUBTRACT:
			list.push_back({ type, { pickVariable(rng, declared), randomOperand(rng, declared), randomOperand(rng, declared) } });
			count--;
			break;
		case InstructionType::SLEEP:
			list.push_back({ type, { uint16_t(randomBetween(rng, 1, MAX_SLEEP)) } });
			count--;
			break;
		case InstructionType::FOR: {
			uint64_t bodySize = randomBetween(rng, 1, min<uint64_t>(count - 1, MAX_FOR_BODY));
			uint16_t repeats = uint16_t(randomBetween(rng, 1, MAX_FOR_REPEATS));
			list.push_back({ type, {}, "", generateList(rng, bodySize, depth + 1, declared), repeats });
			count -= 1 + bodySize;
			break;
		}
		}
	}
	return list;
}

// PRINT, ADD, PRINT, ADD, ... `count` instructions in total.
// x always exists, since every process starts with x = 0.
inline vector<Instruction> generateAlternating(mt19937_64& rng, uint64_t count) {
	vector<Instruction> list;
	list.reserve(count);

	for (uint64_t i = 0; i < count; i++) {
		if (i % 2 == 0) {
			list.push_back({ InstructionType::PRINT, { string("x") }, "Value from: " });
		}
		else {
			uint16_t value = uint16_t(randomBetween(rng, MIN_ADD_VALUE, MAX_ADD_VALUE));
			list.push_back({ InstructionType::ADD, { string("x"), string("x"), value } });
		}
	}
	return list;
}

// A program with between minIns and maxIns instructions
inline vector<Instruction> generateProgram(mt19937_64& rng, uint64_t minIns, uint64_t maxIns) {
	uint64_t count = randomBetween(rng, minIns, maxIns);
	if (ALTERNATING_PROGRAMS) {
		return generateAlternating(rng, count);
	}
	vector<string> declared;
	return generateList(rng, count, 0, declared);
}
