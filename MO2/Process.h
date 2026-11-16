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
#include <algorithm>
#include "Memory.h"
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
	READ,
	WRITE,
	FOR  // keep last: the generator picks a random type from PRINT up to FOR
};

using Operand = variant<string, uint16_t>;

// Each instruction carries its own arguments, so no separate params queue is needed
//   PRINT:         msg, optional args[0] = variable to print after msg
//   DECLARE:       args = { var, value }
//   ADD, SUBTRACT: args = { var1, var2/value, var3/value }
//   SLEEP:         args = { ticks }
//   READ:          args = { var }, address = where to read from
//   WRITE:         args = { var/value }, address = where to write to
//   FOR:           body = instructions to repeat, repeats = loop count
struct Instruction {
	InstructionType type;
	vector<Operand> args;
	string msg;
	vector<Instruction> body;
	uint16_t repeats = 0;
	uint64_t address = 0;  // may lie outside the process's memory; that is checked when it runs
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
	unordered_map<string, uint64_t> symbolTable;  // variable name -> its address; the value is in memory
	vector<Log> outputLog;  // shown only on the attached screen
	uint8_t sleepTicksLeft;
	string createdAt;  // shown by screen -ls and report-util
	uint64_t mem;      // size of this process's memory in bytes; never changes
	Memory* memory;    // set by executeNext, for the instruction it runs

	bool violated;            // shut down by a READ/WRITE outside its memory
	string violationTime;     // "HH:MM:SS"
	uint64_t violationAddress;

	// The first 64 bytes of a process's memory are its symbol table: 32 uint16 variables
	static const size_t MAX_VARIABLES = 32;

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

	bool isDeclared(const string& var) const {
		return symbolTable.find(var) != symbolTable.end();
	}

	// Variable values live in memory, so these can cause page faults
	uint16_t loadVariable(const string& var) {
		return memory->read16(id, symbolTable[var]);
	}

	void storeVariable(const string& var, uint16_t value) {
		memory->write16(id, symbolTable[var], value);
	}

	// Literal values are used as-is; variable names are read from memory
	uint16_t valueOf(const Operand& op) {
		if (holds_alternative<uint16_t>(op)) {
			return get<uint16_t>(op);
		}
		return loadVariable(get<string>(op));
	}

	// Adds the variable names among these operands to `names`
	static void addNames(vector<string>& names, const vector<Operand>& ops) {
		for (const Operand& op : ops) {
			if (holds_alternative<string>(op)) {
				names.push_back(get<string>(op));
			}
		}
	}

	// Declares every listed variable that doesn't exist yet with value 0, each with its
	// own DECLARE log. Returns false, declaring nothing, if the symbol table has no room
	// for all of them; the instruction that needed them is then ignored.
	bool declareMissing(const vector<string>& vars) {
		vector<string> missing;
		for (const string& var : vars) {
			if (!isDeclared(var) && find(missing.begin(), missing.end(), var) == missing.end()) {
				missing.push_back(var);
			}
		}
		if (symbolTable.size() + missing.size() > MAX_VARIABLES) {
			return false;
		}
		for (const string& var : missing) {
			DECLARE(var, 0);
		}
		return true;
	}

	// A uint16 needs 2 bytes, so the last valid address is mem - 2
	bool isValidAddress(uint64_t address) const {
		return mem >= 2 && address <= mem - 2;
	}

	// Shuts the process down; its memory is freed by the scheduler like any finished process
	void accessViolation(InstructionType type, uint64_t address) {
		time_t now = time(nullptr);
		tm local;
#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		stringstream ss;
		ss << put_time(&local, "%H:%M:%S");

		state = TERMINATED;
		violated = true;
		violationTime = ss.str();
		violationAddress = address;
		addLog(type, {}, "Memory access violation at " + toHex(address));
	}

	void PRINT(string msg) {
		if (msg.empty()) {
			msg = "Hello world from " + name;
		}
		// Output goes only to the log: it is shown inside the attached screen (process-smi),
		// and a background scheduler thread must not write to the main console.
		addLog(InstructionType::PRINT, {}, msg);
	}

	// Sets a variable, giving it the next free slot in the symbol table if it is new.
	// Ignored once all 32 slots are taken.
	void DECLARE(string var, uint16_t val) {
		if (!isDeclared(var)) {
			if (symbolTable.size() >= MAX_VARIABLES) {
				addLog(InstructionType::DECLARE, { var, val }, "DECLARE ignored: symbol table is full.");
				return;
			}
			uint64_t address = symbolTable.size() * 2;  // variable n is at address 2n
			symbolTable[var] = address;
		}
		storeVariable(var, val);
		string log = "Variable name: " + var + " with Value: " + to_string(val) + " created.";
		addLog(InstructionType::DECLARE, { var, val }, log);
	}

	void ADD(string var1, Operand op2, Operand op3) {
		vector<string> names = { var1 };
		addNames(names, { op2, op3 });
		if (!declareMissing(names)) {
			addLog(InstructionType::ADD, { var1, op2, op3 }, "ADD ignored: symbol table is full.");
			return;
		}
		uint16_t val2 = valueOf(op2);
		uint16_t val3 = valueOf(op3);

		int result = val2 + val3;
		if (result > UINT16_MAX) {
			result = UINT16_MAX;
		}
		storeVariable(var1, uint16_t(result));
		string log = "SUM: " + to_string(val2) + " + " + to_string(val3) + " = " + to_string(result);
		addLog(InstructionType::ADD, { var1, op2, op3 }, log);
	}

	void SUBTRACT(string var1, Operand op2, Operand op3) {
		vector<string> names = { var1 };
		addNames(names, { op2, op3 });
		if (!declareMissing(names)) {
			addLog(InstructionType::SUBTRACT, { var1, op2, op3 }, "SUBTRACT ignored: symbol table is full.");
			return;
		}
		uint16_t val2 = valueOf(op2);
		uint16_t val3 = valueOf(op3);

		int result = val2 - val3;
		if (result < 0) {
			result = 0;
		}
		storeVariable(var1, uint16_t(result));
		string log = "DIFFERENCE: " + to_string(val2) + " - " + to_string(val3) + " = " + to_string(result);
		addLog(InstructionType::SUBTRACT, { var1, op2, op3 }, log);
	}

	// Reads the uint16 at `address` into var. Memory that was never written reads as 0.
	void READ(string var, uint64_t address) {
		if (!isValidAddress(address)) {
			accessViolation(InstructionType::READ, address);
			return;
		}
		if (!declareMissing({ var })) {
			addLog(InstructionType::READ, { var }, "READ ignored: symbol table is full.");
			return;
		}
		uint16_t value = memory->read16(id, address);
		storeVariable(var, value);
		addLog(InstructionType::READ, { var }, "READ: " + var + " = " + to_string(value) + " from " + toHex(address));
	}

	void WRITE(uint64_t address, Operand op) {
		if (!isValidAddress(address)) {
			accessViolation(InstructionType::WRITE, address);
			return;
		}
		vector<string> names;
		addNames(names, { op });
		if (!declareMissing(names)) {
			addLog(InstructionType::WRITE, { op }, "WRITE ignored: symbol table is full.");
			return;
		}
		uint16_t value = valueOf(op);
		memory->write16(id, address, value);
		addLog(InstructionType::WRITE, { op }, "WRITE: " + to_string(value) + " to " + toHex(address));
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
				else if (!isDeclared(get<string>(op))) {
					msg = "Variable " + get<string>(op) + " does not exist";
				}
				else {
					msg += to_string(loadVariable(get<string>(op)));
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
			SLEEP(uint8_t(get<uint16_t>(instr.args[0])));
			break;
		case InstructionType::READ:
			READ(get<string>(instr.args[0]), instr.address);
			break;
		case InstructionType::WRITE:
			WRITE(instr.address, instr.args[0]);
			break;
		case InstructionType::FOR:
			break;  // handled in nextInstruction()
		}
	}

public:
	Process(int param_id, string param_name, vector<Instruction> param_program, uint64_t param_mem) {
		ctr = 0;
		state = READY;
		core = -1;
		id = param_id;
		name = param_name;
		mem = param_mem;
		memory = nullptr;
		violated = false;
		violationAddress = 0;
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

	// Runs one real instruction. The scheduler calls this once per CPU tick, holding its
	// own lock, which is also what protects `param_memory`.
	void executeNext(Memory& param_memory) {
		lock_guard<mutex> lock(mtx);
		memory = &param_memory;
		const Instruction* instr = nextInstruction();
		if (instr == nullptr) {
			state = TERMINATED;
			return;
		}
		runInstruction(*instr);
		if (violated) {
			return;  // shut down mid-instruction, so it doesn't count as executed
		}
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

	// Safe to read without the mutex, since mem never changes after construction
	uint64_t getMem() const {
		return mem;
	}

	const vector<Log>& getOutputLog() const {
		return outputLog;
	}

	bool isFinished() const {
		return state == TERMINATED;
	}

	bool isViolated() const {
		return violated;
	}

	string getViolationMessage() const {
		return "Process " + name + " shut down due to memory access violation error that occurred at "
			+ violationTime + ". " + toHex(violationAddress) + " invalid.";
	}
};