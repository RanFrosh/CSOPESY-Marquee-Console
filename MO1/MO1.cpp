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
using namespace std;



void enableEscapeCodes();
void clearScreen();
void printHeader();
bool initialize(unique_ptr<ProcessManager>& manager, unique_ptr<Scheduler>& scheduler);
void screen(const vector<string>& commandTexts, ProcessManager& manager, Scheduler& scheduler);
void runProcessScreen(shared_ptr<Process> process);
void processSmi(const Process& process);
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
    cout << "Last updated: November 1, 2026" << endl;
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

// screen -s <name>: create a process and enter its screen
// screen -r <name>: enter an existing, unfinished process's screen
// screen -ls: show CPU utilization and the running and finished processes
void screen(const vector<string>& commandTexts, ProcessManager& manager, Scheduler& scheduler) {
    if (commandTexts.size() == 2 && commandTexts[1] == "-ls") {
        cout << scheduler.buildReport();
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
        scheduler.enqueue(process);
    }
    else {
        process = manager.find(name);

        bool available = process != nullptr;
        if (available) {
            // The scheduler thread may be finishing this process right now
            lock_guard<mutex> lock(process->getMutex());
            available = !process->isFinished();
        }
        if (!available) {
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

    if (process.isFinished()) {
        cout << "Finished!" << endl;
    }
    else {
        cout << "Current instruction line: " << process.getCtr() << endl;
        cout << "Lines of code: " << process.getTotalInstructions() << endl;
    }
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
            screen(commandTexts, *manager, *scheduler);
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
        else if (commandTexts[0] == "exit") {
            break;  // not exit(0), which would skip stopping the scheduler thread
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }

    return 0;
}