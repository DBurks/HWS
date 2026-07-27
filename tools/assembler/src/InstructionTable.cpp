#include "InstructionTable.hpp"
#include <vector>

const std::vector<InstructionSpec>& get_instruction_table() {
    static const std::vector<InstructionSpec> table = {
        // LDA (Load Accumulator)
        {"LDA", AddressingMode::Immediate, 0xA9, 2},
        {"LDA", AddressingMode::ZeroPage,  0xA5, 2},
        {"LDA", AddressingMode::ZeroPageX, 0xB5, 2},
        {"LDA", AddressingMode::Absolute,  0xAD, 3},
        {"LDA", AddressingMode::AbsoluteX, 0xBD, 3},
        {"LDA", AddressingMode::AbsoluteY, 0xB9, 3},
        {"LDA", AddressingMode::IndirectX, 0xA1, 2},
        {"LDA", AddressingMode::IndirectY, 0xB1, 2},

        // STA (Store Accumulator)
        {"STA", AddressingMode::ZeroPage,  0x85, 2},
        {"STA", AddressingMode::ZeroPageX, 0x95, 2},
        {"STA", AddressingMode::Absolute,  0x8D, 3},
        {"STA", AddressingMode::AbsoluteX, 0x9D, 3},
        {"STA", AddressingMode::AbsoluteY, 0x99, 3},
        {"STA", AddressingMode::IndirectX, 0x81, 2},
        {"STA", AddressingMode::IndirectY, 0x91, 2},

        // JMP (Jump)
        {"JMP", AddressingMode::Absolute,  0x4C, 3},
        {"JMP", AddressingMode::Indirect,  0x6C, 3},

        // BRK (Force Interrupt)
        {"BRK", AddressingMode::Implied,   0x00, 1},

        // NOP (No Operation)
        {"NOP", AddressingMode::Implied,   0xEA, 1}
    };
    return table;
}