#include <iostream>
#include <string>

using namespace std;

int main() {
    string command;
    string marqueeText;

    cout << "==============================" << endl;
    cout << "     Main Menu Console" << endl;
    cout << "==============================" << endl;
    cout << "Group developer: Thea, Gerry, Philiipe" << endl;
    cout << "Version date: September 18, 2026" << endl;
    cout << endl;

    // Command loop
    while (command != "exit") {
        cout << "Command> ";
        getline(cin, command);

        if (command == "help") {
            cout << "help - Displays the available commands." << endl;
            cout << "set_text <your_string> - Sets the text for the marquee." << endl;
            cout << "exit - Terminates the console." << endl;
        }
        else if (command.rfind("set_text ", 0) == 0) {
            marqueeText = command.substr(9);
            if(!marqueeText.empty()) {
                cout << "Text accepted for marquee: " << marqueeText << endl; 
            } else {
                cout << "Error: No text provided for marquee." << endl;
            }
        }
        else {
            cout << "Error: Unrecognized command." << endl;
        }
    }

    return 0;
}