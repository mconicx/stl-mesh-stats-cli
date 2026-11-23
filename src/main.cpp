#include <iostream>

#include "../include/stl/mesh.h"

// /home/ozpc/Documents/Temp/cube_ascii.stl
// /home/ozpc/Documents/Temp/cube_binary.stl

int main() {

    std::string szInput;

    std::cout << "stl-mesh-stats-cli Ready. Type 'help' for commands or 'exit' to quit.\n";

    while (true) {

        std::cout << "> ";
        
        if (!std::getline(std::cin, szInput)) break; // Exit cleanly if input stream closes (eg. Ctrl+D / Ctrl+Z)
        std::istringstream iStream(szInput);
        std::string szCmd;
        std::vector<std::string> vecArgs;

        if (iStream >> szCmd) {

            std::string szArg;
            while (iStream >> szArg) {

                vecArgs.push_back(szArg);
            }
        }


        if (szCmd == "exit" || szCmd == "quit") {

            std::cout << "Exiting stl-mesh-stats-cli...\n";
            break;
        } 
        
        if (szCmd == "help") {

            std::cout << "Available commands: exit, help\n";
            std::cout << "exit - Exit the program\n";
            std::cout << "help - Show this screen\n";
            std::cout << "show [f] - Show data for an STL file\n";
            std::cout << "f - Full path to the *.stl file\n";
            continue;
        } 

        if (szCmd == "show") {

            if (vecArgs.size() < 1) {

                std::cout << "Incorrect usage of 'show' command. Usage:\n";
                std::cout << "show [f] - Show data for an STL file\n";
                std::cout << "f - Full path to the *.stl file\n";
                continue;
            }

            CMesh* mesh = new CMesh(vecArgs[0]);
            std::cout << "Loaded mesh from '" + vecArgs[0] + "'\n";
            std::cout << "Data - Type: " + std::string(mesh->GetFormat() == STLFormat::BIN ? "Binary" : "ASCII") + '\n';
            std::cout << "Num of triangles: " + std::to_string(mesh->GetTriangles()->size()) + '\n';
            TBoundingBox bbox = mesh->GetBoundingBox();
            std::cout << "Bounding box: [" 
                + std::string(std::to_string(bbox.Min.fX) + ", " + std::to_string(bbox.Min.fY) + ", " + std::to_string(bbox.Min.fZ)) + 
                "], [" + std::string(std::to_string(bbox.Max.fX) + ", " + std::to_string(bbox.Max.fY) + ", " + std::to_string(bbox.Max.fZ)) + "]\n";
            std::cout << "Surface area: " + std::to_string(mesh->GetSurfaceArea()) + "mm^2\n";
            std::cout << "Volume: " + std::to_string(mesh->GetVolume()) + "mm^3\n";
            std::cout << "Num of degenerate tris: " + std::to_string(mesh->GetNumberOfDegenerateTriangles()) + '\n';
            std::cout << "Is watertight: " + std::string(mesh->IsWaterTight() ? "True" : "False") + '\n';
            continue;
        }
    
        if (!szCmd.empty())
            std::cout << "Unknown command: '" << szCmd << "'\n";
    }

    return 0;
}
