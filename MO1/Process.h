#pragma once
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <mutex>
using namespace std;

enum State {
	READY,
	RUNNING,
	WAITING,
	TERMINATED
};

enum class InstructionType {
	PRINT,
	DECLARE,
	ADD,
	SUBTRACT,
	SLEEP,
	FOR
};

using Operand = variant<string, uint16_t>;

// Each instruction carries its own arguments, so no separate params queue is needed
//   PRINT:         msg, optional args[0] = variable to print after msg
//   DECLARE:       args = { var, value }
//   ADD, SUBTRACT: args = { var1, var2/value, var3/value }
//   SLEEP:         args = { ticks }
//   FOR:           body = instructions to repeat, repeats = loop count
struct Instruction {
	InstructionType type;
	vector<Operand> args;
	string msg;
	vector<Instruction> body;
	uint16_t repeats = 0;
};

// Position inside one list of instructions. The main program is the bottom
// frame; each FOR that starts pushes a frame for its body.
struct Frame {
	const vector<Instruction>* code;
	size_t pc;             // index of the next instruction to run in code
	uint16_t repeatsLeft;  // passes over code still to do, including this one
};

// One entry in a process's output log
struct Log {
	string timestamp;      // e.g. "10/28/2026 04:52:13 PM"
	int core;              // core the process was on when this was logged
	InstructionType type;
	vector<Operand> args;  // copy of the instruction's arguments
	string output;         // readable description of what happened
};

class Process {
private:
	int id;
	string name;
	uint64_t ctr;                // instructions executed so far
	uint64_t totalInstructions;  // instructions the whole program will execute
	State state;
	int core;
	vector<Instruction> program;
	vector<Frame> callStack;
	unordered_map<string, uint16_t> variables;
	vector<Log> outputLog;  // shown only on the attached screen
	uint8_t sleepTicksLeft;
	string createdAt;  // shown by screen -ls and report-util

	// The scheduler thread changes a process while the main thread may be reading it
	// (process-smi). The mutating methods below lock this themselves; a reader on
	// another thread locks it through getMutex().
	mutable mutex mtx;

	// Current local time as "MM/DD/YYYY hh:mm:ss AM"
	static string currentTimestamp() {
		time_t now = time(nullptr);
		tm local;
#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		stringstream ss;
		ss << put_time(&local, "%m/%d/%Y %I:%M:%S %p");
		return ss.str();
	}

	// Counts instructions as executed: a FOR runs its body `repeats` times
	static uint64_t countInstructions(const vector<Instruction>& list) {
		uint64_t count = 0;
		for (const Instruction& instr : list) {
			if (instr.type == InstructionType::FOR) {
				count += instr.repeats * countInstructions(instr.body);
			}
			else {
				count++;
			}
		}
		return count;
	}

	// Adds a log entry stamped with the current time and core
	void addLog(InstructionType type, const vector<Operand>& args, string output = "") {
		outputLog.push_back({ currentTimestamp(), core, type, args, output });
	}

	// Adds a DECLARE log entry var with value 0 if it doesn't exist yet.
	void declareIfMissing(const string& var) {
		if (variables.find(var) == variables.end()) {
			DECLARE(var, 0);
		}
	}

	// Turns an operand into a number: literal values are used as-is, and
	// variable names are looked up (declared with 0 first if missing).
	uint16_t resolve(const Operand& op) {
		if (holds_alternative<uint16_t>(op)) {
			return get<uint16_t>(op);
		}
		const string& var = get<string>(op);
		declareIfMissing(var);
		return variables[var];
	}

	void PRINT(string msg) {
		if (msg.empty()) {
			msg = "Hello world from " + name;
		}
		// Output goes only to the log: it is shown inside the attached screen (process-smi),
		// and a background scheduler thread must not write to the main console.
		addLog(InstructionType::PRINT, {}, msg);
	}

	void DECLARE(string var, uint16_t val) {
		string log = "Variable name: " + var + " with Value: " + to_string(val) + " created.";
		variables[var] = val;
		addLog(InstructionType::DECLARE, {var, val}, log);
	}

	void ADD(string var1, Operand op2, Operand op3) {
		declareIfMissing(var1);
		uint16_t val2 = resolve(op2);
		uint16_t val3 = resolve(op3);

		int result = val2 + val3;
		if (result > UINT16_MAX) {
			result = UINT16_MAX;
		}
		variables[var1] = result;
		string log = "SUM: " + to_string(val2) + " + " + to_string(val3) + " = " + to_string(result);
		addLog(InstructionType::ADD, { var1, op2, op3 }, log);
	}

	void SUBTRACT(string var1, Operand op2, Operand op3) {
		declareIfMissing(var1);
		uint16_t val2 = resolve(op2);
		uint16_t val3 = resolve(op3);

		int result = val2 - val3;
		if (result < 0) {
			result = 0;
		}
		variables[var1] = result;
		string log = "DIFFERENCE: " + to_string(val2) + " - " + to_string(val3) + " = " + to_string(result);
		addLog(InstructionType::SUBTRACT, { var1, op2, op3 }, log);
	}

	void SLEEP(uint8_t duration) {
		state = WAITING;
		sleepTicksLeft = duration;
		addLog(InstructionType::SLEEP, { uint16_t(duration) }, "SLEEP: " + to_string(duration) + " ticks");
	}

	// Walks the frames to the next non-FOR instruction, or nullptr when done
	const Instruction* nextInstruction() {
		while (!callStack.empty()) {
			Frame& top = callStack.back();

			if (top.pc == top.code->size()) {
				top.repeatsLeft--;
				if (top.repeatsLeft > 0) {
					top.pc = 0;
				}
				else {
					callStack.pop_back();
				}
				continue;
			}

			const Instruction& instr = (*top.code)[top.pc];
			top.pc++;

			if (instr.type != InstructionType::FOR) {
				return &instr;
			}
			if (instr.repeats > 0 && !instr.body.empty()) {
				callStack.push_back({ &instr.body, 0, instr.repeats });
			}
		}
		return nullptr;
	}

	void runInstruction(const Instruction& instr) {
		switch (instr.type) {
		case InstructionType::PRINT: {
			// The variable's value is looked up now, when the PRINT runs.
			// Unlike ADD/SUBTRACT, PRINT doesn't declare missing variables.
			string msg = instr.msg;
			if (!instr.args.empty()) {
				const Operand& op = instr.args[0];
				if (holds_alternative<uint16_t>(op)) {
					msg += to_string(get<uint16_t>(op));
				}
				else {
					auto found = variables.find(get<string>(op));
					if (found == variables.end()) {
						msg = "Variable " + get<string>(op) + " does not exist";
					}
					else {
						msg += to_string(found->second);
					}
				}
			}
			PRINT(msg);
			break;
		}
		case InstructionType::DECLARE:
			DECLARE(get<string>(instr.args[0]), get<uint16_t>(instr.args[1]));
			break;
		case InstructionType::ADD:
			ADD(get<string>(instr.args[0]), instr.args[1], instr.args[2]);
			break;
		case InstructionType::SUBTRACT:
			SUBTRACT(get<string>(instr.args[0]), instr.args[1], instr.args[2]);
			break;
		case InstructionType::SLEEP:
			SLEEP(get<uint16_t>(instr.args[0]));
			break;
		case InstructionType::FOR:
			break;  // handled in nextInstruction()
		}
	}

public:
	Process(int param_id, string param_name, vector<Instruction> param_program) {
		ctr = 0;
		state = READY;
		core = -1;
		id = param_id;
		name = param_name;
		program = param_program;
		callStack.push_back({ &program, 0, 1 });
		sleepTicksLeft = 0;
		createdAt = currentTimestamp();
		totalInstructions = countInstructions(program);
		if (totalInstructions == 0) {
			state = TERMINATED;
		}
	}

	// callStack points into program, so a Process must not be copied.
	// Keep processes behind pointers instead, e.g. shared_ptr<Process>.
	Process(const Process&) = delete;
	Process& operator=(const Process&) = delete;

	// Runs one real instruction. The scheduler calls this once per CPU tick.
	void executeNext() {
		lock_guard<mutex> lock(mtx);
		const Instruction* instr = nextInstruction();
		if (instr == nullptr) {
			state = TERMINATED;
			return;
		}
		runInstruction(*instr);
		ctr++;

		// Finish right after the last instruction instead of on an extra tick
		if (ctr == totalInstructions) {
			state = TERMINATED;
		}
	}

	// Called by the scheduler once per CPU tick while this process is sleeping
	void tickSleep() {
		lock_guard<mutex> lock(mtx);
		if (sleepTicksLeft > 0) {
			sleepTicksLeft--;
		}
		if (sleepTicksLeft == 0) {
			state = READY;
		}
	}

	// Called by the scheduler when it puts this process on a core
	void assignCore(int param_core) {
		lock_guard<mutex> lock(mtx);
		core = param_core;
		state = RUNNING;
	}

	// Called by the scheduler when it takes this process off its core.
	// A process that fell asleep or finished keeps that state.
	void releaseCore() {
		lock_guard<mutex> lock(mtx);
		core = -1;
		if (state == RUNNING) {
			state = READY;
		}
	}

	// Lock this around any read of the log, counters or state from a non-scheduler thread
	mutex& getMutex() const {
		return mtx;
	}

	string getCreatedAt() const {
		return createdAt;
	}

	int getId() const {
		return id;
	}

	string getName() const {
		return name;
	}

	State getState() const {
		return state;
	}

	int getCore() const {
		return core;
	}

	uint64_t getCtr() const {
		return ctr;
	}

	uint64_t getTotalInstructions() const {
		return totalInstructions;
	}

	const vector<Log>& getOutputLog() const {
		return outputLog;
	}

	bool isFinished() const {
		return state == TERMINATED;
	}
};