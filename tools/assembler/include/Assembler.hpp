#pragma once

#include <string>
#include <vector>
#include <cstdint>

class Assembler {
public:
    // Parses and assembles a source file into raw machine code bytes
    bool assemble_file(const std::string& filepath, std::vector<uint8_t>& out_machine_code);
};