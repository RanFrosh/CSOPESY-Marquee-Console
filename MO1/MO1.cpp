#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "Process.h"
using namespace std;



void initialize();
void screen();
void scheduler_start();
void scheduler_stop();
void report_util();
vector<string> parse(string input);



void initialize() {
    cout << "Ran initialize!" << endl;
}

void screen() {
    cout << "Ran screen!" << endl;
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

    cout << "Tokens" << endl;
    for (int i = 0; i < tokens.size(); i++) {
        
        cout << tokens[i] << endl;
    }
    
    return tokens;
}


int main() {
	string input;
    vector<string> commandTexts;

    cout << "==============================" << endl;
    cout << "     Main Menu Console" << endl;
    cout << "==============================" << endl;
    cout << "Welcome to CSOPESY Emulator!" << endl;
    cout << "Group developers: \nChu, Avery\nSia, Justin\nTan, Paul\nTiu, Avram\n" << endl;
    cout << "Last updated: October 27, 2026" << endl;
    cout << "------------------------------" << endl;
    cout << endl;

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
            screen();
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