#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include <concepts>

// Structural Register File Maps
enum class Reg6502 : uint8_t {
    A = 0, // Accumulator
    X,     // X Index
    Y,     // Y Index
    SP,    // Stack Pointer
    Count
};

// Status Register (P) Flag Bitmasks
struct Flag6502 {
    static constexpr uint8_t Carry     = 1 << 0;
    static constexpr uint8_t Zero      = 1 << 1;
    static constexpr uint8_t Interrupt = 1 << 2;
    static constexpr uint8_t Decimal   = 1 << 3;
    static constexpr uint8_t Break     = 1 << 4;
    static constexpr uint8_t Unused    = 1 << 5; // Always 1
    static constexpr uint8_t Overflow  = 1 << 6;
    static constexpr uint8_t Negative  = 1 << 7;
};

// 6502 Addressing Modes
enum class AddressingMode : uint8_t {
    IMP,  // Implied (no operand, e.g., NOP, TAX, CLC)
    IMM,  // Immediate (#$xx — 1 byte value follows opcode)
    ZP,   // Zero Page ($xx — 1 byte address in 0x00-0xFF range)
    ZPX,  // Zero Page,X ($xx,X — 1 byte address + X register offset)
    ZPY,  // Zero Page,Y ($xx,Y — 1 byte address + Y register offset)
    ABS,  // Absolute ($xxxx — 2 byte full address, low byte first)
    ABX,  // Absolute,X ($xxxx,X — 2 byte address + X register offset)
    ABY,  // Absolute,Y ($xxxx,Y — 2 byte address + Y register offset)
    IND,  // Indirect ($xxxx) — 2 byte pointer to a 2 byte address
    IZX,  // (Indirect,X) — 1 byte zero-page pointer pre-indexed by X
    IZY,  // (Indirect),Y — 1 byte zero-page pointer post-indexed by Y
    REL,  // Relative — 1 byte signed offset for branches
    ACC   // Accumulator (operates on A register, e.g., ASL A)
};

// 6502 Instructions (56 total mnemonics)
enum class Instruction : uint8_t {
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS,
    CLC, CLD, CLI, CLV, CMP, CPX, CPY,
    DEC, DEX, DEY,
    EOR,
    INC, INX, INY,
    JMP, JSR,
    LDA, LDX, LDY, LSR,
    NOP,
    ORA,
    PHA, PHP, PLA, PLP,
    ROL, ROR, RTI, RTS,
    SBC, SEC, SED, SEI, STA, STX, STY,
    TAX, TAY, TSX, TXA, TXS, TYA,
    XXX  // Sentinel for illegal/undefined opcodes
};

// Returns the total instruction byte length for a given addressing mode
// This tells the CPU how many bytes to consume from the instruction stream
constexpr uint8_t addressing_mode_bytes(AddressingMode mode) {
    switch (mode) {
        case AddressingMode::IMP: return 1;  // Just the opcode
        case AddressingMode::ACC: return 1;  // Just the opcode
        case AddressingMode::IMM: return 2;  // Opcode + 1 byte value (#$xx)
        case AddressingMode::ZP:  return 2;  // Opcode + 1 byte address ($xx)
        case AddressingMode::ZPX: return 2;  // Opcode + 1 byte address ($xx,X)
        case AddressingMode::ZPY: return 2;  // Opcode + 1 byte address ($xx,Y)
        case AddressingMode::REL: return 2;  // Opcode + 1 byte signed offset
        case AddressingMode::IZX: return 2;  // Opcode + 1 byte pointer ($xx,X)
        case AddressingMode::IZY: return 2;  // Opcode + 1 byte pointer ($xx),Y
        case AddressingMode::ABS: return 3;  // Opcode + 2 byte address ($xxxx)
        case AddressingMode::ABX: return 3;  // Opcode + 2 byte address ($xxxx,X)
        case AddressingMode::ABY: return 3;  // Opcode + 2 byte address ($xxxx,Y)
        case AddressingMode::IND: return 3;  // Opcode + 2 byte pointer ($xxxx)
    }
    return 1; // unreachable
}

// Opcode table entry: instruction + addressing mode + base cycle count
// Byte length is computed from the addressing mode via addressing_mode_bytes()
struct OpcodeInfo {
    Instruction    instr;
    AddressingMode mode;
    uint8_t        cycles;
};

// Sentinel for illegal/undefined opcodes
constexpr OpcodeInfo kIllegal = {Instruction::XXX, AddressingMode::IMP, 0};

// 16x16 Opcode Matrix (high nibble = row, low nibble = col)
// Matches the masswerk 6502 reference layout exactly
constexpr std::array<std::array<OpcodeInfo, 16>, 16> kOpcodeTable = {{

    // Row 0x0
    {{ {Instruction::BRK, AddressingMode::IMP, 7},   // 0x00
       {Instruction::ORA, AddressingMode::IZX, 6},   // 0x01
       kIllegal,                                      // 0x02
       kIllegal,                                      // 0x03
       kIllegal,                                      // 0x04
       {Instruction::ORA, AddressingMode::ZP,  3},    // 0x05
       {Instruction::ASL, AddressingMode::ZP,  5},    // 0x06
       kIllegal,                                      // 0x07
       {Instruction::PHP, AddressingMode::IMP, 3},    // 0x08
       {Instruction::ORA, AddressingMode::IMM, 2},    // 0x09
       {Instruction::ASL, AddressingMode::ACC, 2},    // 0x0A
       kIllegal,                                      // 0x0B
       kIllegal,                                      // 0x0C
       {Instruction::ORA, AddressingMode::ABS, 4},    // 0x0D
       {Instruction::ASL, AddressingMode::ABS, 6},    // 0x0E
       kIllegal }},                                   // 0x0F

    // Row 0x1
    {{ {Instruction::BPL, AddressingMode::REL, 2},   // 0x10
       {Instruction::ORA, AddressingMode::IZY, 5},   // 0x11
       kIllegal,                                      // 0x12
       kIllegal,                                      // 0x13
       kIllegal,                                      // 0x14
       {Instruction::ORA, AddressingMode::ZPX, 4},   // 0x15
       {Instruction::ASL, AddressingMode::ZPX, 6},   // 0x16
       kIllegal,                                      // 0x17
       {Instruction::CLC, AddressingMode::IMP, 2},   // 0x18
       {Instruction::ORA, AddressingMode::ABY, 4},   // 0x19
       kIllegal,                                      // 0x1A
       kIllegal,                                      // 0x1B
       kIllegal,                                      // 0x1C
       {Instruction::ORA, AddressingMode::ABX, 4},   // 0x1D
       {Instruction::ASL, AddressingMode::ABX, 7},   // 0x1E
       kIllegal }},                                   // 0x1F

    // Row 0x2
    {{ {Instruction::JSR, AddressingMode::ABS, 6},   // 0x20
       {Instruction::AND, AddressingMode::IZX, 6},   // 0x21
       kIllegal,                                      // 0x22
       kIllegal,                                      // 0x23
       {Instruction::BIT, AddressingMode::ZP,  3},    // 0x24
       {Instruction::AND, AddressingMode::ZP,  3},    // 0x25
       {Instruction::ROL, AddressingMode::ZP,  5},    // 0x26
       kIllegal,                                      // 0x27
       {Instruction::PLP, AddressingMode::IMP, 4},    // 0x28
       {Instruction::AND, AddressingMode::IMM, 2},    // 0x29
       {Instruction::ROL, AddressingMode::ACC, 2},    // 0x2A
       kIllegal,                                      // 0x2B
       {Instruction::BIT, AddressingMode::ABS, 4},    // 0x2C
       {Instruction::AND, AddressingMode::ABS, 4},    // 0x2D
       {Instruction::ROL, AddressingMode::ABS, 6},    // 0x2E
       kIllegal }},                                   // 0x2F

    // Row 0x3
    {{ {Instruction::BMI, AddressingMode::REL, 2},   // 0x30
       {Instruction::AND, AddressingMode::IZY, 5},   // 0x31
       kIllegal,                                      // 0x32
       kIllegal,                                      // 0x33
       kIllegal,                                      // 0x34
       {Instruction::AND, AddressingMode::ZPX, 4},   // 0x35
       {Instruction::ROL, AddressingMode::ZPX, 6},   // 0x36
       kIllegal,                                      // 0x37
       {Instruction::SEC, AddressingMode::IMP, 2},   // 0x38
       {Instruction::AND, AddressingMode::ABY, 4},   // 0x39
       kIllegal,                                      // 0x3A
       kIllegal,                                      // 0x3B
       kIllegal,                                      // 0x3C
       {Instruction::AND, AddressingMode::ABX, 4},   // 0x3D
       {Instruction::ROL, AddressingMode::ABX, 7},   // 0x3E
       kIllegal }},                                   // 0x3F

    // Row 0x4
    {{ {Instruction::RTI, AddressingMode::IMP, 6},   // 0x40
       {Instruction::EOR, AddressingMode::IZX, 6},   // 0x41
       kIllegal,                                      // 0x42
       kIllegal,                                      // 0x43
       kIllegal,                                      // 0x44
       {Instruction::EOR, AddressingMode::ZP,  3},    // 0x45
       {Instruction::LSR, AddressingMode::ZP,  5},    // 0x46
       kIllegal,                                      // 0x47
       {Instruction::PHA, AddressingMode::IMP, 3},    // 0x48
       {Instruction::EOR, AddressingMode::IMM, 2},    // 0x49
       {Instruction::LSR, AddressingMode::ACC, 2},    // 0x4A
       kIllegal,                                      // 0x4B
       {Instruction::JMP, AddressingMode::ABS, 3},    // 0x4C
       {Instruction::EOR, AddressingMode::ABS, 4},    // 0x4D
       {Instruction::LSR, AddressingMode::ABS, 6},    // 0x4E
       kIllegal }},                                   // 0x4F

    // Row 0x5
    {{ {Instruction::BVC, AddressingMode::REL, 2},   // 0x50
       {Instruction::EOR, AddressingMode::IZY, 5},   // 0x51
       kIllegal,                                      // 0x52
       kIllegal,                                      // 0x53
       kIllegal,                                      // 0x54
       {Instruction::EOR, AddressingMode::ZPX, 4},   // 0x55
       {Instruction::LSR, AddressingMode::ZPX, 6},   // 0x56
       kIllegal,                                      // 0x57
       {Instruction::CLI, AddressingMode::IMP, 2},   // 0x58
       {Instruction::EOR, AddressingMode::ABY, 4},   // 0x59
       kIllegal,                                      // 0x5A
       kIllegal,                                      // 0x5B
       kIllegal,                                      // 0x5C
       {Instruction::EOR, AddressingMode::ABX, 4},   // 0x5D
       {Instruction::LSR, AddressingMode::ABX, 7},   // 0x5E
       kIllegal }},                                   // 0x5F

    // Row 0x6
    {{ {Instruction::RTS, AddressingMode::IMP, 6},   // 0x60
       {Instruction::ADC, AddressingMode::IZX, 6},   // 0x61
       kIllegal,                                      // 0x62
       kIllegal,                                      // 0x63
       kIllegal,                                      // 0x64
       {Instruction::ADC, AddressingMode::ZP,  3},    // 0x65
       {Instruction::ROR, AddressingMode::ZP,  5},    // 0x66
       kIllegal,                                      // 0x67
       {Instruction::PLA, AddressingMode::IMP, 4},    // 0x68
       {Instruction::ADC, AddressingMode::IMM, 2},    // 0x69
       {Instruction::ROR, AddressingMode::ACC, 2},    // 0x6A
       kIllegal,                                      // 0x6B
       {Instruction::JMP, AddressingMode::IND, 5},    // 0x6C
       {Instruction::ADC, AddressingMode::ABS, 4},    // 0x6D
       {Instruction::ROR, AddressingMode::ABS, 6},    // 0x6E
       kIllegal }},                                   // 0x6F

    // Row 0x7
    {{ {Instruction::BVS, AddressingMode::REL, 2},   // 0x70
       {Instruction::ADC, AddressingMode::IZY, 5},   // 0x71
       kIllegal,                                      // 0x72
       kIllegal,                                      // 0x73
       kIllegal,                                      // 0x74
       {Instruction::ADC, AddressingMode::ZPX, 4},   // 0x75
       {Instruction::ROR, AddressingMode::ZPX, 6},   // 0x76
       kIllegal,                                      // 0x77
       {Instruction::SEI, AddressingMode::IMP, 2},   // 0x78
       {Instruction::ADC, AddressingMode::ABY, 4},   // 0x79
       kIllegal,                                      // 0x7A
       kIllegal,                                      // 0x7B
       kIllegal,                                      // 0x7C
       {Instruction::ADC, AddressingMode::ABX, 4},   // 0x7D
       {Instruction::ROR, AddressingMode::ABX, 7},   // 0x7E
       kIllegal }},                                   // 0x7F

    // Row 0x8
    {{ kIllegal,                                      // 0x80
       {Instruction::STA, AddressingMode::IZX, 6},   // 0x81
       kIllegal,                                      // 0x82
       kIllegal,                                      // 0x83
       {Instruction::STY, AddressingMode::ZP,  3},    // 0x84
       {Instruction::STA, AddressingMode::ZP,  3},    // 0x85
       {Instruction::STX, AddressingMode::ZP,  3},    // 0x86
       kIllegal,                                      // 0x87
       {Instruction::DEY, AddressingMode::IMP, 2},    // 0x88
       kIllegal,                                      // 0x89
       {Instruction::TXA, AddressingMode::IMP, 2},    // 0x8A
       kIllegal,                                      // 0x8B
       {Instruction::STY, AddressingMode::ABS, 4},    // 0x8C
       {Instruction::STA, AddressingMode::ABS, 4},    // 0x8D
       {Instruction::STX, AddressingMode::ABS, 4},    // 0x8E
       kIllegal }},                                   // 0x8F

    // Row 0x9
    {{ {Instruction::BCC, AddressingMode::REL, 2},   // 0x90
       {Instruction::STA, AddressingMode::IZY, 6},   // 0x91
       kIllegal,                                      // 0x92
       kIllegal,                                      // 0x93
       {Instruction::STY, AddressingMode::ZPX, 4},   // 0x94
       {Instruction::STA, AddressingMode::ZPX, 4},   // 0x95
       {Instruction::STX, AddressingMode::ZPY, 4},   // 0x96
       kIllegal,                                      // 0x97
       {Instruction::TYA, AddressingMode::IMP, 2},    // 0x98
       {Instruction::STA, AddressingMode::ABY, 5},    // 0x99
       {Instruction::TXS, AddressingMode::IMP, 2},    // 0x9A
       kIllegal,                                      // 0x9B
       kIllegal,                                      // 0x9C
       {Instruction::STA, AddressingMode::ABX, 5},    // 0x9D
       kIllegal,                                      // 0x9E
       kIllegal }},                                   // 0x9F

    // Row 0xA
    {{ {Instruction::LDY, AddressingMode::IMM, 2},   // 0xA0
       {Instruction::LDA, AddressingMode::IZX, 6},   // 0xA1
       {Instruction::LDX, AddressingMode::IMM, 2},   // 0xA2
       kIllegal,                                      // 0xA3
       {Instruction::LDY, AddressingMode::ZP,  3},    // 0xA4
       {Instruction::LDA, AddressingMode::ZP,  3},    // 0xA5
       {Instruction::LDX, AddressingMode::ZP,  3},    // 0xA6
       kIllegal,                                      // 0xA7
       {Instruction::TAY, AddressingMode::IMP, 2},    // 0xA8
       {Instruction::LDA, AddressingMode::IMM, 2},    // 0xA9
       {Instruction::TAX, AddressingMode::IMP, 2},    // 0xAA
       kIllegal,                                      // 0xAB
       {Instruction::LDY, AddressingMode::ABS, 4},    // 0xAC
       {Instruction::LDA, AddressingMode::ABS, 4},    // 0xAD
       {Instruction::LDX, AddressingMode::ABS, 4},    // 0xAE
       kIllegal }},                                   // 0xAF

    // Row 0xB
    {{ {Instruction::BCS, AddressingMode::REL, 2},   // 0xB0
       {Instruction::LDA, AddressingMode::IZY, 5},   // 0xB1
       kIllegal,                                      // 0xB2
       kIllegal,                                      // 0xB3
       {Instruction::LDY, AddressingMode::ZPX, 4},   // 0xB4
       {Instruction::LDA, AddressingMode::ZPX, 4},   // 0xB5
       {Instruction::LDX, AddressingMode::ZPY, 4},   // 0xB6
       kIllegal,                                      // 0xB7
       {Instruction::CLV, AddressingMode::IMP, 2},   // 0xB8
       {Instruction::LDA, AddressingMode::ABY, 4},   // 0xB9
       {Instruction::TSX, AddressingMode::IMP, 2},   // 0xBA
       kIllegal,                                      // 0xBB
       {Instruction::LDY, AddressingMode::ABX, 4},   // 0xBC
       {Instruction::LDA, AddressingMode::ABX, 4},   // 0xBD
       {Instruction::LDX, AddressingMode::ABY, 4},   // 0xBE
       kIllegal }},                                   // 0xBF

    // Row 0xC
    {{ {Instruction::CPY, AddressingMode::IMM, 2},   // 0xC0
       {Instruction::CMP, AddressingMode::IZX, 6},   // 0xC1
       kIllegal,                                      // 0xC2
       kIllegal,                                      // 0xC3
       {Instruction::CPY, AddressingMode::ZP,  3},    // 0xC4
       {Instruction::CMP, AddressingMode::ZP,  3},    // 0xC5
       {Instruction::DEC, AddressingMode::ZP,  5},    // 0xC6
       kIllegal,                                      // 0xC7
       {Instruction::INY, AddressingMode::IMP, 2},    // 0xC8
       {Instruction::CMP, AddressingMode::IMM, 2},    // 0xC9
       {Instruction::DEX, AddressingMode::IMP, 2},    // 0xCA
       kIllegal,                                      // 0xCB
       {Instruction::CPY, AddressingMode::ABS, 4},    // 0xCC
       {Instruction::CMP, AddressingMode::ABS, 4},    // 0xCD
       {Instruction::DEC, AddressingMode::ABS, 6},    // 0xCE
       kIllegal }},                                   // 0xCF

    // Row 0xD
    {{ {Instruction::BNE, AddressingMode::REL, 2},   // 0xD0
       {Instruction::CMP, AddressingMode::IZY, 5},   // 0xD1
       kIllegal,                                      // 0xD2
       kIllegal,                                      // 0xD3
       kIllegal,                                      // 0xD4
       {Instruction::CMP, AddressingMode::ZPX, 4},   // 0xD5
       {Instruction::DEC, AddressingMode::ZPX, 6},   // 0xD6
       kIllegal,                                      // 0xD7
       {Instruction::CLD, AddressingMode::IMP, 2},   // 0xD8
       {Instruction::CMP, AddressingMode::ABY, 4},   // 0xD9
       kIllegal,                                      // 0xDA
       kIllegal,                                      // 0xDB
       kIllegal,                                      // 0xDC
       {Instruction::CMP, AddressingMode::ABX, 4},   // 0xDD
       {Instruction::DEC, AddressingMode::ABX, 7},   // 0xDE
       kIllegal }},                                   // 0xDF

    // Row 0xE
    {{ {Instruction::CPX, AddressingMode::IMM, 2},   // 0xE0
       {Instruction::SBC, AddressingMode::IZX, 6},   // 0xE1
       kIllegal,                                      // 0xE2
       kIllegal,                                      // 0xE3
       {Instruction::CPX, AddressingMode::ZP,  3},    // 0xE4
       {Instruction::SBC, AddressingMode::ZP,  3},    // 0xE5
       {Instruction::INC, AddressingMode::ZP,  5},    // 0xE6
       kIllegal,                                      // 0xE7
       {Instruction::INX, AddressingMode::IMP, 2},    // 0xE8
       {Instruction::SBC, AddressingMode::IMM, 2},    // 0xE9
       {Instruction::NOP, AddressingMode::IMP, 2},    // 0xEA
       kIllegal,                                      // 0xEB
       {Instruction::CPX, AddressingMode::ABS, 4},    // 0xEC
       {Instruction::SBC, AddressingMode::ABS, 4},    // 0xED
       {Instruction::INC, AddressingMode::ABS, 6},    // 0xEE
       kIllegal }},                                   // 0xEF

    // Row 0xF
    {{ {Instruction::BEQ, AddressingMode::REL, 2},   // 0xF0
       {Instruction::SBC, AddressingMode::IZY, 5},   // 0xF1
       kIllegal,                                      // 0xF2
       kIllegal,                                      // 0xF3
       kIllegal,                                      // 0xF4
       {Instruction::SBC, AddressingMode::ZPX, 4},   // 0xF5
       {Instruction::INC, AddressingMode::ZPX, 6},   // 0xF6
       kIllegal,                                      // 0xF7
       {Instruction::SED, AddressingMode::IMP, 2},   // 0xF8
       {Instruction::SBC, AddressingMode::ABY, 4},   // 0xF9
       kIllegal,                                      // 0xFA
       kIllegal,                                      // 0xFB
       kIllegal,                                      // 0xFC
       {Instruction::SBC, AddressingMode::ABX, 4},   // 0xFD
       {Instruction::INC, AddressingMode::ABX, 7},   // 0xFE
       kIllegal }}                                    // 0xFF

}};

// Compile-Time Policy Configuration Tag for the KIM-1 Layout
struct KIM1_Config {
   static constexpr bool kEnableTrace = 
#ifdef HWS_ENABLE_TRACE
    HWS_ENABLE_TRACE;
#else
    false;
#endif
    using AddrType = uint16_t;
    using DataType = uint8_t;
    using RegEnum  = Reg6502;
    
    static constexpr size_t RegisterCount = static_cast<size_t>(Reg6502::Count);
    static constexpr AddrType ResetVector = 0x0400;
    static constexpr AddrType StackBase   = 0x0100;
};

// Defined here so any file including configs can immediately check the interface contract
template <typename T, typename Config>
concept SystemBusType = requires(T bus, typename Config::AddrType addr, typename Config::DataType data, uint32_t& cycles) {
    { bus.read(addr, cycles) } -> std::same_as<typename Config::DataType>;
    { bus.write(addr, data, cycles) } -> std::same_as<void>;
};