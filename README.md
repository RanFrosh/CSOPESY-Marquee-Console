# Marquee Console

## CSOPESY S01 Group 10

-   Chu, Avery Simone B.
-   Sia, Justin Michael R.
-   Tan, Paul Aiden G.
-   Tiu, Avram Nathaniel P.

## File Structure

This project contains the following folders:

-   **CLI Exercise**: a simple marquee console
-   **M03**: a marquee console with animation and functions
-   **M04**: a desktop-style OS mock-up
-   **MO1**: a process scheduler emulator with FCFS and round robin scheduling
-   **MO2**: the MO1 emulator with demand paging memory management

## Compiling & Running Instructions

The entry class file `main.cpp` is located in the **M03** folder.

To compile and run the program, enter the following command in the command line or terminal. <br>
`g++ main.cpp -o main.exe`

---

The entry class file `M04.cpp` is located in the **M04** folder.

To compile and run the program on **Visual Studio**, run M04.cpp after letting CMakeLists.txt cache.
Set Visual Studio config json variable CMakeList to true.

### Compiling and Running on MacOS

To compile and run the program on the terminal, enter the following commands line or terminal. <br>
`cmake -S .-B build` <br> `cmake --build build`

---

The entry class file `MO1.cpp` is located in the **MO1** folder.

To compile and run the program on **Visual Studio**, run MO1.cpp after letting CMakeLists.txt cache.
Set Visual Studio config json variable CMakeList to true.

To compile and run the program on the terminal, enter the following command in the command line or terminal. <br>
`g++ -std=c++20 MO1.cpp -o MO1.exe`

`config.txt` must be in the same folder the program is run from.

---

The entry class file `MO2.cpp` is located in the **MO2** folder.

To compile and run the program on **Visual Studio**, run MO2.cpp after letting CMakeLists.txt cache.
Set Visual Studio config json variable CMakeList to true.

To compile and run the program on the terminal, enter the following command in the command line or terminal. <br>
`g++ -std=c++20 MO2.cpp -o MO2.exe`

`config.txt` must be in the same folder the program is run from. The program creates `csopesy-backing-store.txt` in that same folder.