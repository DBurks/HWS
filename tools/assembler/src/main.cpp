#include "Assembler.hpp"
#include <iostream>
#include <vector>
#include <fstream>

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 2) {
        std::cerr << "Usage: " << args[0] << " <input_file.asm> [output_file.bin]\n";
        return 1;
    }

    std::string input_file = args[1];
    std::string output_file = (args.size() >= 3) ? args[2] : "program.bin";

    Assembler assembler;
    std::vector<uint8_t> machine_code_bytes;

    if (!assembler.assemble_file(input_file, machine_code_bytes)) {
        std::cerr << "Assembly failed.\n";
        return 1;
    }

    // Write raw binary bytes to output file
    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_file << "\n";
        return 1;
    }
    
    outfile.write(reinterpret_cast<const char*>(machine_code_bytes.data()), machine_code_bytes.size());

    std::cout << "Successfully assembled " << input_file << " -> " << output_file 
              << " (" << machine_code_bytes.size() << " bytes).\n";

    return 0;
}