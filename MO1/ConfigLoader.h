#pragma once
#include <string>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include "Config.h"
using namespace std;

const uint64_t CONFIG_MAX = 4294967296ULL;  // 2^32

// Removes one pair of surrounding double quotes: "rr" -> rr
inline string stripQuotes(string text) {
	if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
		return text.substr(1, text.size() - 2);
	}
	return text;
}

// Parses a whole-number string and checks it is within [lo, hi].
inline bool parseNumber(const string& text, uint64_t lo, uint64_t hi, uint64_t& out) {
	if (text.empty() || text.size() > 19) {
		return false;
	}
	for (char c : text) {
		if (!isdigit(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	uint64_t value = stoull(text);
	if (value < lo || value > hi) {
		return false;
	}
	out = value;
	return true;
}

// Reads the space-separated "key value" pairs of config.txt into `out`.
// Returns false and sets `error` if the file is missing, has an unknown or
// missing parameter, or a value is out of range. `out` is only changed on success.
inline bool loadConfig(const string& path, Config& out, string& error) {
	ifstream file(path);
	if (!file) {
		error = "Could not open " + path + ".";
		return false;
	}

	const vector<string> required = {
		"num-cpu", "scheduler", "quantum-cycles", "batch-process-freq",
		"min-ins", "max-ins", "delay-per-exec"
	};

	unordered_map<string, string> values;
	string key, value;
	while (file >> key >> value) {
		if (key == "delays-per-exec") {
			key = "delay-per-exec";  // the spec's table and sample use different spellings
		}
		if (find(required.begin(), required.end(), key) == required.end()) {
			error = "Unknown parameter \"" + key + "\" in " + path + ".";
			return false;
		}
		values[key] = stripQuotes(value);
	}

	for (const string& name : required) {
		if (values.find(name) == values.end()) {
			error = "Missing parameter \"" + name + "\" in " + path + ".";
			return false;
		}
	}

	Config result;
	uint64_t n = 0;

	if (!parseNumber(values["num-cpu"], 1, 128, n)) {
		error = "num-cpu must be a whole number from 1 to 128.";
		return false;
	}
	result.numCpu = static_cast<int>(n);

	string scheduler = values["scheduler"];
	transform(scheduler.begin(), scheduler.end(), scheduler.begin(),
		[](unsigned char c) { return static_cast<char>(tolower(c)); });
	if (scheduler != "fcfs" && scheduler != "rr") {
		error = "scheduler must be \"fcfs\" or \"rr\".";
		return false;
	}
	result.scheduler = scheduler;

	if (!parseNumber(values["quantum-cycles"], 1, CONFIG_MAX, result.quantumCycles)) {
		error = "quantum-cycles must be a whole number from 1 to 2^32.";
		return false;
	}
	if (!parseNumber(values["batch-process-freq"], 1, CONFIG_MAX, result.batchProcessFreq)) {
		error = "batch-process-freq must be a whole number from 1 to 2^32.";
		return false;
	}
	if (!parseNumber(values["min-ins"], 1, CONFIG_MAX, result.minIns)) {
		error = "min-ins must be a whole number from 1 to 2^32.";
		return false;
	}
	if (!parseNumber(values["max-ins"], 1, CONFIG_MAX, result.maxIns)) {
		error = "max-ins must be a whole number from 1 to 2^32.";
		return false;
	}
	if (result.minIns > result.maxIns) {
		error = "min-ins must not be greater than max-ins.";
		return false;
	}
	if (!parseNumber(values["delay-per-exec"], 0, CONFIG_MAX, result.delaysPerExec)) {
		error = "delay-per-exec must be a whole number from 0 to 2^32.";
		return false;
	}

	out = result;
	return true;
}