#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread> 
#include <algorithm>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace std;

const string SPACES(100, ' ');
const int DISPLAY_SIZE = 44;

mutex out_mtx;    // protects cout
mutex text_mtx;   // protects marquee text
string marquee = "CSOPESY";

//atomic elements for multithread safe use of a variable
atomic<bool> running(true);     // program alive or not
atomic<bool> animating(true);   // marquee moving or not
atomic<int> speed_ms(400);

// draw a line on row 1 without disturbing the user's cursor
void draw(const string& line) {
    lock_guard<mutex> lock(out_mtx);

    //save current cursor position , print, then restore cursor position
    cout << "\033[s\033[1;1H" << line << "\033[u" << flush;
}

// sleeps in small stops so stop/exit is responsive
void wait_frame(int ms) {
    while (ms > 0 && running) {
        int stop = min(ms, 50);
        this_thread::sleep_for(chrono::milliseconds(stop));
        ms -= stop;
    }
}

// blocks while stopped; returns false if program is exiting
bool frame_ready() {
    while (running && !animating)
        this_thread::sleep_for(chrono::milliseconds(50));
    return running;
}

void marquee_display() {
    while (running) {
        string text;
        { lock_guard<mutex> l(text_mtx); text = marquee; }
        int len = (int)text.length();

        // text appearing
        for (int i = 0; i <= DISPLAY_SIZE; i++) {
            if (!frame_ready()) return;

            wait_frame(speed_ms);
            string lead(DISPLAY_SIZE - i, ' ');
            string trail(max(i - len, 0), ' ');
            int shown = min(i, len);
            draw(lead + text.substr(0, shown) + trail + SPACES);
        }
        // text disappearing
        for (int i = 0; i < len; i++) {
            if (!frame_ready()) return;

            wait_frame(speed_ms);
            draw(text.substr(i) + SPACES);
        }
        // pause
        for (int i = 0; i < 10; i++) {
            if (!frame_ready()) return;

            wait_frame(speed_ms);
            draw(SPACES);
        }
    }
}

void print_help() {
    lock_guard<mutex> lock(out_mtx);
    cout << "help - displays the commands and its description\n"
        "start_marquee - starts the marquee \"animation\"\n"
        "stop_marquee - stops the marquee \"animation\"\n"
        "set_text - accepts a text input and displays it as a marquee\n"
        "set_speed - sets the marquee animation refresh in milliseconds\n"
        "exit - terminates the console\n";
}

void print_error() {
    lock_guard<mutex> lock(out_mtx);
    cout << "\033[15;1HERROR! Command not recognized.\033[13;1H";
}

void command() {
    //display command text and accept input
    {
        lock_guard<mutex> lock(out_mtx);
        cout << "\033[13;1HCommand> " << SPACES << "\033[13;10H" << flush;
    }
    string temp;
    getline(cin, temp);  

    // clear lines
    {
        lock_guard<mutex> lock(out_mtx);
        for (int i = 14; i < 20; i++) cout << "\033[" << i << ";1H" << SPACES;
        cout << "\033[13;1H";
    }
	
	string nospace = temp;
	nospace.erase(remove(nospace.begin(), nospace.end(), ' '), nospace.end());

    if (temp == "help") print_help();
    else if (temp == "start_marquee") animating = true;
    else if (temp == "stop_marquee") animating = false;
    else if (temp.rfind("set_text ", 0) == 0 && nospace.length() > 8) {
        lock_guard<mutex> l(text_mtx);
        marquee = temp.substr(9);
    }
    else if (temp.rfind("set_speed ", 0) == 0) {
            //find the indicated ms after the set_speed
        try { speed_ms = max(10, stoi(temp.substr(10))); }
        catch (...) { print_error(); }
    }
    else if (temp == "exit") running = false;
    else print_error();
}

int main() {
//enables \033 escape codes
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    //print welcomes
    {
        lock_guard<mutex> lock(out_mtx);
        cout << "\033[2J\033[3;1H"
            << "Welcome to CSOPESY!\n\n"
            << "Group developers:\n"
            << "Chu, Avery\nSia, Justin\nTan, Paul\nTiu, Avram\n\n"
            << "Version date: 2026-09-20\n";
    }

    //run marquee display in separate thread
    thread marquee_thread(marquee_display);

    //keep command prompting running while user has not yet exited
    while (running) command();

    //wait on marquee thread before closing exe
    marquee_thread.join();
    cout << "\033[21;1H";
    return 0;
}