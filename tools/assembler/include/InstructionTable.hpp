#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class AddressingMode {
    Implied,
    Accumulator,
    Immediate,
    ZeroPage,
    ZeroPageX,
    ZeroPageY,
    Absolute,
    AbsoluteX,
    AbsoluteY,
    Indirect,
    IndirectX,
    IndirectY,
    Relative
};

struct InstructionSpec {
    std::string mnemonic;
    AddressingMode mode;
    uint8_t opcode;
    uint8_t bytes;
};

// Declare the lookup table function
const std::vector<InstructionSpec>& get_instruction_table();