#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <cstdint>
#include "Process.h"
#include "Generator.h"  // MAX_FOR_DEPTH, shared with random programs
using namespace std;

// screen -c accepts 1 to 50 instructions; a FOR counts as 1 plus everything in its body
const uint64_t MIN_USER_INSTRUCTIONS = 1;
const uint64_t MAX_USER_INSTRUCTIONS = 50;

// Turns screen -c's instruction text into a program, the same Instruction structs
// that generateProgram builds. It reads the text left to right with one shared
// position; a FOR parses its body by calling the list parser again, so FORs can nest.
//
//   program := instr ( ";" instr )* [";"]
//   instr   := DECLARE name number
//            | ADD name operand operand | SUBTRACT name operand operand
//            | SLEEP number
//            | READ name address | WRITE address operand
//            | PRINT "(" [ string ] [ "+" name ] ")"   (or PRINT(name))
//            | FOR "(" "[" instr ( "," instr )* "]" "," number ")"
//   operand := number | name
//   address := hex, e.g. 0x500 (whether it is inside the process's memory is checked when it runs)
//
// Names are letters, digits and _, not starting with a digit, so "10" is always a number.
class InstructionParser {
private:
	const string& text;
	size_t pos;

	void skipSpaces() {
		while (pos < text.size() && isspace(static_cast<unsigned char>(text[pos]))) {
			pos++;
		}
	}

	bool atEnd() {
		skipSpaces();
		return pos == text.size();
	}

	// True if `c` is next, without consuming it
	bool peek(char c) {
		skipSpaces();
		return pos < text.size() && text[pos] == c;
	}

	// Consumes `c` if it is next
	bool accept(char c) {
		if (!peek(c)) {
			return false;
		}
		pos++;
		return true;
	}

	bool readName(string& out) {
		skipSpaces();
		size_t start = pos;
		if (pos < text.size() && (isalpha(static_cast<unsigned char>(text[pos])) || text[pos] == '_')) {
			pos++;
			while (pos < text.size() && (isalnum(static_cast<unsigned char>(text[pos])) || text[pos] == '_')) {
				pos++;
			}
		}
		if (pos == start) {
			return false;
		}
		out = text.substr(start, pos - start);
		return true;
	}

	// A whole number from 0 to max
	bool readNumber(uint64_t max, uint64_t& out) {
		skipSpaces();
		size_t start = pos;
		while (pos < text.size() && isdigit(static_cast<unsigned char>(text[pos]))) {
			pos++;
		}
		if (pos == start || pos - start > 19) {
			return false;
		}
		uint64_t value = stoull(text.substr(start, pos - start));
		if (value > max) {
			return false;
		}
		out = value;
		return true;
	}

	// 0x followed by 1 to 16 hex digits
	bool readAddress(uint64_t& out) {
		skipSpaces();
		if (pos + 1 >= text.size() || text[pos] != '0' || (text[pos + 1] != 'x' && text[pos + 1] != 'X')) {
			return false;
		}
		size_t start = pos + 2;
		size_t end = start;
		while (end < text.size() && isxdigit(static_cast<unsigned char>(text[end]))) {
			end++;
		}
		if (end == start || end - start > 16) {
			return false;
		}
		out = stoull(text.substr(start, end - start), nullptr, 16);
		pos = end;
		return true;
	}

	bool readOperand(Operand& out) {
		skipSpaces();
		if (pos < text.size() && isdigit(static_cast<unsigned char>(text[pos]))) {
			uint64_t value = 0;
			if (!readNumber(UINT16_MAX, value)) {
				return false;
			}
			out = uint16_t(value);
			return true;
		}
		string name;
		if (!readName(name)) {
			return false;
		}
		out = name;
		return true;
	}

	// "text": everything up to the next quote, so ; , ] ) inside are just text
	bool readString(string& out) {
		if (!accept('"')) {
			return false;
		}
		size_t end = text.find('"', pos);
		if (end == string::npos) {
			return false;
		}
		out = text.substr(pos, end - pos);
		pos = end + 1;
		return true;
	}

	bool parsePrint(vector<Instruction>& list) {
		string msg;
		vector<Operand> args;

		if (!accept('(')) {
			return false;
		}
		if (peek('"')) {
			if (!readString(msg)) {
				return false;
			}
			if (accept('+')) {
				string var;
				if (!readName(var)) {
					return false;
				}
				args.push_back(var);
			}
		}
		else if (!peek(')')) {
			string var;  // PRINT(varA)
			if (!readName(var)) {
				return false;
			}
			args.push_back(var);
		}
		if (!accept(')')) {
			return false;
		}

		list.push_back({ InstructionType::PRINT, args, msg });
		return true;
	}

	// The opening FOR has already been read. `depth` is how many FORs this one is inside.
	bool parseFor(int depth, vector<Instruction>& list) {
		Instruction loop{ InstructionType::FOR };
		uint64_t repeats = 0;

		if (depth >= MAX_FOR_DEPTH) {
			return false;
		}
		if (!accept('(') || !accept('[')) {
			return false;
		}
		if (!parseList(depth + 1, ',', loop.body)) {
			return false;
		}
		if (!accept(']') || !accept(',')) {
			return false;
		}
		if (!readNumber(UINT16_MAX, repeats) || repeats == 0) {
			return false;
		}
		if (!accept(')')) {
			return false;
		}

		loop.repeats = uint16_t(repeats);
		list.push_back(loop);
		return true;
	}

	bool parseInstruction(int depth, vector<Instruction>& list) {
		string keyword;
		if (!readName(keyword)) {
			return false;
		}

		if (keyword == "DECLARE") {
			string var;
			uint64_t value = 0;
			if (!readName(var) || !readNumber(UINT16_MAX, value)) {
				return false;
			}
			list.push_back({ InstructionType::DECLARE, { var, uint16_t(value) } });
			return true;
		}
		if (keyword == "ADD" || keyword == "SUBTRACT") {
			string var;
			Operand op2, op3;
			if (!readName(var) || !readOperand(op2) || !readOperand(op3)) {
				return false;
			}
			auto type = keyword == "ADD" ? InstructionType::ADD : InstructionType::SUBTRACT;
			list.push_back({ type, { var, op2, op3 } });
			return true;
		}
		if (keyword == "SLEEP") {
			uint64_t ticks = 0;
			if (!readNumber(UINT8_MAX, ticks)) {
				return false;
			}
			list.push_back({ InstructionType::SLEEP, { uint16_t(ticks) } });
			return true;
		}
		if (keyword == "READ") {
			string var;
			uint64_t address = 0;
			if (!readName(var) || !readAddress(address)) {
				return false;
			}
			list.push_back({ InstructionType::READ, { var }, "", {}, 0, address });
			return true;
		}
		if (keyword == "WRITE") {
			uint64_t address = 0;
			Operand value;
			if (!readAddress(address) || !readOperand(value)) {
				return false;
			}
			list.push_back({ InstructionType::WRITE, { value }, "", {}, 0, address });
			return true;
		}
		if (keyword == "PRINT") {
			return parsePrint(list);
		}
		if (keyword == "FOR") {
			return parseFor(depth, list);
		}
		return false;  // unknown keyword
	}

	// Instructions separated by `separator`: ";" at the top level, "," inside a FOR.
	// A separator with nothing after it is allowed, so a trailing ";" is fine.
	bool parseList(int depth, char separator, vector<Instruction>& list) {
		do {
			if (!parseInstruction(depth, list)) {
				return false;
			}
		} while (accept(separator) && !atEnd());
		return true;
	}

	// Instructions as written: a FOR is 1 plus its body, the same as min-ins/max-ins
	static uint64_t countWritten(const vector<Instruction>& list) {
		uint64_t count = 0;
		for (const Instruction& instr : list) {
			count += 1;
			if (instr.type == InstructionType::FOR) {
				count += countWritten(instr.body);
			}
		}
		return count;
	}

public:
	InstructionParser(const string& param_text) : text(param_text), pos(0) {
	}

	// Fills `out` and returns true only if the whole text is valid and has 1 to 50 instructions
	bool parse(vector<Instruction>& out) {
		vector<Instruction> program;
		if (!parseList(0, ';', program) || !atEnd()) {
			return false;
		}
		uint64_t count = countWritten(program);
		if (count < MIN_USER_INSTRUCTIONS || count > MAX_USER_INSTRUCTIONS) {
			return false;
		}
		out = program;
		return true;
	}
};
