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
#include "Config.h"
#include "Process.h"
#include "ProcessManager.h"
using namespace std;



void enableEscapeCodes();
void clearScreen();
void printHeader();
void initialize();
void screen(const vector<string>& commandTexts, ProcessManager& manager);
void runProcessScreen(shared_ptr<Process> process);
void processSmi(const Process& process);
void scheduler_start();
void scheduler_stop();
void report_util();
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
    cout << "Last updated: October 27, 2026" << endl;
    cout << "------------------------------" << endl;
    cout << endl;
}

void initialize() {
    cout << "Ran initialize!" << endl;
}

// screen -s <name>: create a process and enter its screen
// screen -r <name>: enter an existing, unfinished process's screen
void screen(const vector<string>& commandTexts, ProcessManager& manager) {
    if (commandTexts.size() == 2 && commandTexts[1] == "-ls") {
        cout << "screen -ls is not available yet." << endl;  // needs the scheduler
        return;
    }
    if (commandTexts.size() < 3 || (commandTexts[1] != "-s" && commandTexts[1] != "-r")) {
        cout << "Usage: screen -s <process name> | screen -r <process name> | screen -ls" << endl;
        return;
    }

    const string& name = commandTexts[2];
    shared_ptr<Process> process;

    if (commandTexts[1] == "-s") {
        process = manager.createProcess(name);
        if (process == nullptr) {
            cout << "Process " << name << " already exists." << endl;
            return;
        }
        // Once the scheduler exists, add the process to its ready queue here
    }
    else {
        process = manager.find(name);
        if (process == nullptr || process->isFinished()) {
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
    cout << "Process name: " << process.getName() << endl;
    cout << "ID: " << process.getId() << endl;
    cout << "Logs:" << endl;
    for (const Log& log : process.getOutputLog()) {
        if (log.type == InstructionType::PRINT) {
            cout << "(" << log.timestamp << ") Core: " << log.core << " \"" << log.output << "\"" << endl;
        }
    }
    cout << endl;

    if (process.isFinished()) {
        cout << "Finished!" << endl;
    }
    else {
        cout << "Current instruction line: " << process.getCtr() << endl;
        cout << "Lines of code: " << process.getTotalInstructions() << endl;
    }
}

void scheduler_start() {
    cout << "Ran scheduler start!" << endl;
}

void scheduler_stop() {
    cout << "Ran scheduler stop!" << endl;
}

void report_util() {
    cout << "Ran report util!" << endl;
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

    // Filled in by hand until initialize reads config.txt
    Config config{ 4, "fcfs", 1, 1, 5, 20, 0 };
    ProcessManager manager(config);

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

        if (commandTexts[0] == "initialize") {
            initialize();
        }
        else if (commandTexts[0] == "screen") {
            screen(commandTexts, manager);
        }
        else if (commandTexts[0] == "scheduler-start") {
            scheduler_start();
        }
        else if (commandTexts[0] == "scheduler-stop") {
            scheduler_stop();
        }
        else if (commandTexts[0] == "report-util") {
            report_util();
        }
        else if (commandTexts[0] == "exit") {
            exit(0);
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }

    return 0;
}