// Launcher.cpp : Ce fichier contient la fonction 'main'. L'exécution du programme commence et se termine à cet endroit.
//

#include <iostream>
#include <windows.h>
#include <vector>
#include <string>

using namespace std;

int main(int argc, char** argv) {
    if (argc > 1)
        for (int i = 1; i < argc; i++) {
            string str = "\"" + string(argv[i]) + "\"";
            ShellExecuteA(NULL, "open", "LearningPlanning.exe", str.c_str(), NULL, SW_SHOWDEFAULT);
        }
    else
        ShellExecuteA(NULL, "open", "LearningPlanning.exe", NULL, NULL, SW_SHOWDEFAULT);
}
