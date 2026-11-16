#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>  // must come before the "using namespace std" in the headers below
#endif
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <mutex>
#include <fstream>
#include "Config.h"
#include "ConfigLoader.h"
#include "Process.h"
#include "ProcessManager.h"
#include "Scheduler.h"
#include "InstructionParser.h"
using namespace std;



void enableEscapeCodes();
void clearScreen();
void printHeader();
bool initialize(unique_ptr<ProcessManager>& manager, unique_ptr<Scheduler>& scheduler);
bool readUserProgram(const string& input, vector<Instruction>& program);
void screen(const vector<string>& commandTexts, const string& input, ProcessManager& manager, Scheduler& scheduler);
void runProcessScreen(shared_ptr<Process> process);
void processSmi(const Process& process);
void processSmiMain(Scheduler& scheduler);
void vmstat(Scheduler& scheduler);
void scheduler_start(Scheduler& scheduler);
void scheduler_stop(Scheduler& scheduler);
void report_util(Scheduler& scheduler);
vector<string> parse(string input);



// Older Windows consoles print escape codes as text unless this is turned on
void enableEscapeCodes() {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(console, &mode)) {
        SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

// Clears the console and moves the cursor to the top left
void clearScreen() {
    cout << "\033[2J\033[H";
}

void printHeader() {
    cout << "==============================" << endl;
    cout << "     Main Menu Console" << endl;
    cout << "==============================" << endl;
    cout << "Welcome to CSOPESY Emulator!" << endl;
    cout << "Group developers: \nChu, Avery\nSia, Justin\nTan, Paul\nTiu, Avram\n" << endl;
    cout << "Last updated: November 16, 2026" << endl;
    cout << "------------------------------" << endl;
    cout << endl;
}

// Reads config.txt, then creates the process manager and starts the CPU thread.
// Returns true only if config.txt was read successfully.
bool initialize(unique_ptr<ProcessManager>& manager, unique_ptr<Scheduler>& scheduler) {
    Config config;
    string error;

    if (!loadConfig("config.txt", config, error)) {
        cout << "Error: " << error << endl;
        return false;
    }

    manager = make_unique<ProcessManager>(config);
    scheduler = make_unique<Scheduler>(config, *manager);
    scheduler->start();

    cout << "Initialized: " << config.numCpu << " CPU(s), " << config.scheduler << " scheduler." << endl;
    return true;
}

// The program for screen -c, read from the raw input line because parse() would split
// the quoted instructions apart at their spaces. Everything after the memory size must be
// one quoted string; quotes inside it are typed as \" and turned back into plain quotes.
bool readUserProgram(const string& input, vector<Instruction>& program) {
    stringstream ss(input);
    string skipped;
    for (int i = 0; i < 4; i++) {
        ss >> skipped;  // screen -c <name> <memory size>
    }
    string rest;
    getline(ss, rest);

    size_t first = rest.find_first_not_of(" \t");
    size_t last = rest.find_last_not_of(" \t");
    if (first == string::npos || first == last || rest[first] != '"' || rest[last] != '"') {
        return false;
    }

    string text;
    for (size_t i = first + 1; i < last; i++) {
        if (rest[i] == '\\' && i + 1 < last && rest[i + 1] == '"') {
            i++;
        }
        text += rest[i];
    }

    return InstructionParser(text).parse(program);
}

// screen -s <name> <memory size>: create a process with that many bytes and enter its screen
// screen -c <name> <memory size> "<instructions>": the same, but running the given instructions
// screen -r <name>: enter an existing, unfinished process's screen
// screen -ls: show CPU utilization and the running and finished processes
void screen(const vector<string>& commandTexts, const string& input, ProcessManager& manager, Scheduler& scheduler) {
    if (commandTexts.size() == 2 && commandTexts[1] == "-ls") {
        cout << scheduler.buildReport();
        return;
    }

    bool create = commandTexts.size() >= 4 && commandTexts[1] == "-s";
    bool custom = commandTexts.size() >= 5 && commandTexts[1] == "-c";
    bool reattach = commandTexts.size() >= 3 && commandTexts[1] == "-r";
    if (!create && !custom && !reattach) {
        cout << "Usage:" << endl;
        cout << "  screen -s <process name> <memory size>" << endl;
        cout << "  screen -c <process name> <memory size> \"<instructions>\"" << endl;
        cout << "  screen -r <process name>" << endl;
        cout << "  screen -ls" << endl;
        return;
    }

    const string& name = commandTexts[2];
    shared_ptr<Process> process;

    if (create || custom) {
        // A power of 2 from 2^6 to 2^16 bytes
        uint64_t mem = 0;
        if (!parseNumber(commandTexts[3], MIN_PROC_MEM, MAX_PROC_MEM, mem) || !isPowerOfTwo(mem)) {
            cout << "invalid memory allocation" << endl;
            return;
        }

        vector<Instruction> program;
        if (custom && !readUserProgram(input, program)) {
            cout << "invalid command" << endl;
            return;
        }

        // With demand paging, creation never fails for lack of memory: pages load as they are used
        if (custom) {
            process = manager.createCustomProcess(name, mem, program);
        }
        else {
            process = manager.createProcess(name, mem);
        }
        if (process == nullptr) {
            cout << "Process " << name << " already exists." << endl;
            return;
        }
        scheduler.enqueue(process);
    }
    else {
        process = manager.find(name);
        if (process == nullptr) {
            cout << "Process " << name << " not found." << endl;
            return;
        }

        // The scheduler thread may be finishing this process right now
        lock_guard<mutex> lock(process->getMutex());
        if (process->isViolated()) {
            cout << process->getViolationMessage() << endl;
            return;
        }
        if (process->isFinished()) {
            cout << "Process " << name << " not found." << endl;
            return;
        }
    }

    runProcessScreen(process);
}

// The process screen's own command loop; returns to the main menu on "exit"
void runProcessScreen(shared_ptr<Process> process) {
    string input;
    vector<string> commandTexts;

    clearScreen();
    cout << "Process name: " << process->getName() << endl;
    cout << "ID: " << process->getId() << endl;

    while (true) {
        cout << "\n" << process->getName() << " (ID: " << process->getId() << "):\\> ";
        // Stop when input ends
        if (!getline(cin, input)) {
            return;
        }
        commandTexts = parse(input);

        if (commandTexts.empty()) {
            continue;
        }

        if (commandTexts[0] == "process-smi") {
            processSmi(*process);
        }
        else if (commandTexts[0] == "exit") {
            clearScreen();
            printHeader();
            return;
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }
}

void processSmi(const Process& process) {
    // The scheduler thread may be running this process right now
    lock_guard<mutex> lock(process.getMutex());

    cout << "Process name: " << process.getName() << endl;
    cout << "ID: " << process.getId() << endl;
    cout << "Logs:" << endl;
    for (const Log& log : process.getOutputLog()) {
        if (log.type == InstructionType::PRINT) {
            cout << "(" << log.timestamp << ") Core: " << log.core << " \"" << log.output << "\"" << endl;
        }
    }
    cout << endl;

    if (process.isViolated()) {
        cout << process.getViolationMessage() << endl;
    }
    else if (process.isFinished()) {
        cout << "Finished!" << endl;
    }
    else {
        cout << "Current instruction line: " << process.getCtr() << endl;
        cout << "Lines of code: " << process.getTotalInstructions() << endl;
    }
}

// The main menu's process-smi: CPU and memory use, and the memory of each running process.
// Inside a process screen, process-smi calls processSmi instead.
void processSmiMain(Scheduler& scheduler) {
    cout << scheduler.buildMemoryReport();
}

// Memory and per-core CPU tick totals
void vmstat(Scheduler& scheduler) {
    cout << scheduler.buildVmstat();
}

void scheduler_start(Scheduler& scheduler) {
    if (scheduler.startGenerating()) {
        cout << "Generating dummy processes." << endl;
    }
    else {
        cout << "Already generating dummy processes." << endl;
    }
}

void scheduler_stop(Scheduler& scheduler) {
    if (scheduler.stopGenerating()) {
        cout << "Stopped generating dummy processes." << endl;
    }
    else {
        cout << "Not generating dummy processes." << endl;
    }
}

// Saves the same information as screen -ls to a file
void report_util(Scheduler& scheduler) {
    ofstream file("csopesy-log.txt");
    if (!file.is_open()) {
        cout << "Error: Could not open csopesy-log.txt." << endl;
        return;
    }

    file << scheduler.buildReport();
    cout << "Report saved to csopesy-log.txt." << endl;
}

vector<string> parse(string input) {
    string word;
    stringstream ss(input);
    vector<string> tokens;

    while (ss >> word) {
        tokens.push_back(word);
    }

    return tokens;
}


int main() {
	string input;
    vector<string> commandTexts;

    // Both stay empty until initialize succeeds. The scheduler uses the manager,
    // so it is declared after it and gets destroyed (and stopped) first.
    unique_ptr<ProcessManager> manager;
    unique_ptr<Scheduler> scheduler;
    bool initialized = false;

    enableEscapeCodes();
    printHeader();

    // Command loop
    while (commandTexts.size() == 0 || commandTexts[0] != "exit") {
        cout << "\nroot:\\> ";
        // Stop when input ends
        if (!getline(cin, input)) {
            break;
        }
        commandTexts = parse(input);

        // Prevent crashing on empty text
        if (commandTexts.empty()) {
            continue;
        }

        // Only initialize and exit work until config.txt has been read successfully
        if (!initialized && commandTexts[0] != "initialize" && commandTexts[0] != "exit") {
            cout << "Error: Please run initialize first." << endl;
            continue;
        }

        if (commandTexts[0] == "initialize") {
            if (initialized) {
                cout << "Error: Already initialized." << endl;
            }
            else if (initialize(manager, scheduler)) {
                initialized = true;
            }
        }
        else if (commandTexts[0] == "screen") {
            screen(commandTexts, input, *manager, *scheduler);
        }
        else if (commandTexts[0] == "scheduler-start") {
            scheduler_start(*scheduler);
        }
        else if (commandTexts[0] == "scheduler-stop") {
            scheduler_stop(*scheduler);
        }
        else if (commandTexts[0] == "report-util") {
            report_util(*scheduler);
        }
        else if (commandTexts[0] == "process-smi") {
            processSmiMain(*scheduler);
        }
        else if (commandTexts[0] == "vmstat") {
            vmstat(*scheduler);
        }
        else if (commandTexts[0] == "exit") {
            break;  // not exit(0), which would skip stopping the scheduler thread
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }

    return 0;
}