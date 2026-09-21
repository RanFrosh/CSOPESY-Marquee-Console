#include <iostream>
#include <string>
#include <algorithm>

using namespace std;

int main() {
    string command;
    string marqueeText;

    cout << "==============================" << endl;
    cout << "     Main Menu Console" << endl;
    cout << "==============================" << endl;   
    cout << "Group developer: \nChu, Avery\nSia, Justin\nTan, Paul\nTiu, Avram\n" << endl;
    cout << "Version date: September 21, 2026" << endl;
    cout << endl;

    // Command loop
    while (command != "exit") {
        cout << "Command> ";
        getline(cin, command);

        if (command == "help") {
            cout << "help - Displays the available commands." << endl;
            cout << "set_text <your_string> - Sets the text for the marquee." << endl;
            cout << "exit - Terminates the console." << endl;
			cout << "NOTE: These commands below won't work for this exercise" << endl;
			cout << "start_marquee - Starts the marquee animation." << endl;
			cout << "stop_marquee - Stops the marquee animation." << endl;
			cout << "set_speed <speed_value> - Sets the speed of the marquee animation." << endl;
        }
        else if (command.rfind("set_text ", 0) == 0) {
            marqueeText = command.substr(9);
            string nospace = command;
            nospace.erase(remove(nospace.begin(), nospace.end(), ' '), nospace.end());

            if (!marqueeText.empty() && nospace.length() > 8) {
                cout << "Text accepted for marquee: " << marqueeText << endl;
            }
            else {
                cout << "Error: No text provided for marquee." << endl;
            }
        }
        else if (command == "exit") {
            exit(0);
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }

    return 0;
}