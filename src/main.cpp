#include <iostream>

int main() {

    std::string szCmd = "";

    std::cout << "stl-mesh-stats-cli Ready. Type 'help' for commands or 'exit' to quit.\n";

    while (true) {

        std::cout << "> ";
        
        if (!std::getline(std::cin, szCmd)) break; // Exit cleanly if input stream closes (eg. Ctrl+D / Ctrl+Z)

        if (szCmd == "exit" || szCmd == "quit") {

            std::cout << "Exiting stl-mesh-stats-cli...\n";
            break;
        } 
        
        if (szCmd == "help") {

            std::cout << "Available commands: help, exit\n";
            continue;
        } 
    
        if (!szCmd.empty()) {

            std::cout << "Unknown command: '" << szCmd << "'\n";
            continue;
        }
    }

    return 0;
}
