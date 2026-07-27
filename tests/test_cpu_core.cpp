#include <gtest/gtest.h>
#include "CPUCore.hpp"
#include "SystemBus.hpp"
#include "PlatformConfig.hpp"

// ============================================================
// Existing Tests (from Phase 1 scaffolding)
// ============================================================

TEST(MOS6502CoreTest, VerifySystemResetVectorRedirection) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu; 

    test_bus.write_raw(0xFFFC, 0x00); // Low byte of Reset Vector
    test_bus.write_raw(0xFFFD, 0x04); // High byte

    uint32_t reset_cycles = 0;
    test_cpu.reset(test_bus, reset_cycles); // Trigger reset sequence

    EXPECT_EQ(test_cpu.get_pc(), KIM1_Config::ResetVector);
    EXPECT_EQ(reset_cycles, 7u); // Reset sequence takes exactly 7 cycles
}

TEST(MOS6502CoreTest, VerifyNOPExecutionCycleWeight) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu; 

    typename KIM1_Config::AddrType current_pc = test_cpu.get_pc();
    test_bus.write_raw(current_pc, 0xEA); // Inject NOP

    uint32_t cycle_accumulator = 0;
    test_cpu.step(test_bus, cycle_accumulator);

    EXPECT_EQ(cycle_accumulator, 2u); // 1 fetch + 1 internal exec
    EXPECT_EQ(test_cpu.get_pc(), current_pc + 1);
}

TEST(MOS6502CoreTest, VerifyLDAImmediateExecutionAndFlags) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    typename KIM1_Config::AddrType current_pc = test_cpu.get_pc();
    test_bus.write_raw(current_pc, 0xA9);     // LDA Immediate Opcode
    test_bus.write_raw(current_pc + 1, 0x8F); // Negative immediate value (bit 7 set)

    uint32_t cycle_accumulator = 0;
    test_cpu.step(test_bus, cycle_accumulator);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x8F);
    EXPECT_EQ(cycle_accumulator, 2u); // 1 fetch opcode + 1 fetch operand
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Negative));
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Zero));
}

// ============================================================
// JMP Indirect Page-Wrap Bug Tests
// ============================================================
// The 6502 has a famous bug: when JMP ($xxFF) is used (i.e., the
// pointer address ends in $FF), the CPU fetches the high byte from
// $xx00 instead of $(xx+1)00. It wraps within the same 256-byte page.

TEST(MOS6502CoreTest, JMPIndirectPageWrapBug) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    // Place the JMP indirect instruction at $0200
    test_cpu.set_pc(0x0200);

    // JMP ($30FF) — opcode 0x6C, followed by low/high of pointer address
    test_bus.write_raw(0x0200, 0x6C); // JMP Indirect
    test_bus.write_raw(0x0201, 0xFF); // pointer low  = $FF
    test_bus.write_raw(0x0202, 0x30); // pointer high = $30  → pointer = $30FF

    // Set up the pointer values at $30FF and $3000 (the wrap-around address)
    test_bus.write_raw(0x30FF, 0x34); // low byte  of target at $30FF
    test_bus.write_raw(0x3000, 0x12); // high byte of target at $3000 (wrapped!)
    // If the bug weren't present, high byte would come from $3100

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // Expected: PC = $1234 (low from $30FF, high from $3000 due to wrap bug)
    EXPECT_EQ(test_cpu.get_pc(), 0x1234);
    EXPECT_EQ(cycles, 5u); // JMP indir base cycles
}

TEST(MOS6502CoreTest, JMPIndirectNoWrapNormal) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);

    // JMP ($30FE) — pointer at $30FE, no page wrap
    test_bus.write_raw(0x0200, 0x6C);
    test_bus.write_raw(0x0201, 0xFE);
    test_bus.write_raw(0x0202, 0x30);

    // Set up pointer values at $30FE and $30FF (normal, no wrap)
    test_bus.write_raw(0x30FE, 0x78); // low byte
    test_bus.write_raw(0x30FF, 0x56); // high byte → target = $5678

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_pc(), 0x5678);
    EXPECT_EQ(cycles, 5u);
}

// ============================================================
// Page-Cross Penalty Tests (ABX / ABY)
// ============================================================
// When using Absolute,X or Absolute,Y addressing, if the base address
// and the indexed address are on different 256-byte pages, the CPU
// adds 1 extra cycle.

TEST(MOS6502CoreTest, LDAAbsoluteXPageCrossPenalty) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    // Set up: LDA $20FF,X with X=1 → effective address = $2100 (page crossed)
    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0x01);

    test_bus.write_raw(0x0200, 0xBD); // LDA Absolute,X
    test_bus.write_raw(0x0201, 0xFF); // low byte  of base = $FF
    test_bus.write_raw(0x0202, 0x20); // high byte of base = $20 → base = $20FF
    test_bus.write_raw(0x2100, 0xAB); // value at $2100 (page crossed)

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0xAB);
    // LDA abs,X base = 4 cycles + 1 page cross = 5
    EXPECT_EQ(cycles, 5u);
}

TEST(MOS6502CoreTest, LDAAbsoluteXNoPageCross) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    // LDA $2000,X with X=1 → effective address = $2001 (same page)
    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0x01);

    test_bus.write_raw(0x0200, 0xBD);
    test_bus.write_raw(0x0201, 0x00);
    test_bus.write_raw(0x0202, 0x20);
    test_bus.write_raw(0x2001, 0xCD);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0xCD);
    EXPECT_EQ(cycles, 4u); // No page cross, base 4 cycles
}

TEST(MOS6502CoreTest, LDAAbsoluteYPageCrossPenalty) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    // LDA $20FF,Y with Y=1 → effective address = $2100 (page crossed)
    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::Y, 0x01);

    test_bus.write_raw(0x0200, 0xB9); // LDA Absolute,Y
    test_bus.write_raw(0x0201, 0xFF);
    test_bus.write_raw(0x0202, 0x20);
    test_bus.write_raw(0x2100, 0x42);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x42);
    EXPECT_EQ(cycles, 5u); // 4 base + 1 page cross
}

// ============================================================
// Transfer Instruction Tests
// ============================================================

TEST(MOS6502CoreTest, TAXTransferAndFlags) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x80); // Negative value

    test_bus.write_raw(0x0200, 0xAA); // TAX

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::X), 0x80);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Negative));
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Zero));
    EXPECT_EQ(cycles, 2u);
}

TEST(MOS6502CoreTest, TAYZeroFlag) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x00);

    test_bus.write_raw(0x0200, 0xA8); // TAY

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::Y), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Negative));
}

TEST(MOS6502CoreTest, TXATSX) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0x42);

    test_bus.write_raw(0x0200, 0x8A); // TXA
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x42);

    test_cpu.set_pc(0x0201);
    test_bus.write_raw(0x0201, 0x9A); // TXS
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0x42);
}

// ============================================================
// Stack Instruction Tests
// ============================================================

TEST(MOS6502CoreTest, PHAPLA) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t reset_cycles = 0;
    test_cpu.reset(test_bus, reset_cycles); // Sets SP to 0xFD

    test_cpu.set_pc(0x0200);

    uint32_t cycles = 0;
    // Set up test code at PC
    test_bus.write(0x0200, 0x48, cycles); // PHA
    test_bus.write(0x0201, 0x68, cycles); // PLA
    test_cpu.set_reg(Reg6502::A, 0xDE);

    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0xFC);
    EXPECT_EQ(test_bus.read_raw(0x01FD), 0xDE); // Pushed to $01FD

    // Now PLA it back
    test_cpu.set_reg(Reg6502::A, 0x00); // Clear A
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0xDE);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0xFD);
}

TEST(MOS6502CoreTest, PHPPLP) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t reset_cycles = 0;
    test_cpu.reset(test_bus, reset_cycles); // Sets SP to 0xFD
    test_cpu.set_pc(0x0200);
    
    uint32_t cycles = 0;
    // Set some flags
    test_cpu.set_flag(Flag6502::Carry, true);
    test_cpu.set_flag(Flag6502::Negative, true);

    test_bus.write_raw(0x0200, 0x08); // PHP
    test_bus.write_raw(0x0201, 0x28); // PLP

    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0xFC);

    // Clear flags, then PLP to restore
    test_cpu.set_flag(Flag6502::Carry, false);
    test_cpu.set_flag(Flag6502::Negative, false);

    test_cpu.step(test_bus, cycles);

    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Negative));
}

// ============================================================
// Increment / Decrement Tests
// ============================================================

TEST(MOS6502CoreTest, INXDEX) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0xFF);

    test_bus.write_raw(0x0200, 0xE8); // INX
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::X), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));

    test_cpu.set_pc(0x0201);
    test_bus.write_raw(0x0201, 0xCA); // DEX
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::X), 0xFF);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Negative));
}

TEST(MOS6502CoreTest, INYDEY) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::Y, 0x00);

    test_bus.write_raw(0x0200, 0xC8); // INY
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::Y), 0x01);

    test_cpu.set_pc(0x0201);
    test_bus.write_raw(0x0201, 0x88); // DEY
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::Y), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

// ============================================================
// ALU Instruction Tests
// ============================================================

TEST(MOS6502CoreTest, ANDImmediate) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0xFF);

    test_bus.write_raw(0x0200, 0x29); // AND #$0F
    test_bus.write_raw(0x0201, 0x0F);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x0F);
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Negative));
}

TEST(MOS6502CoreTest, ORAImmediate) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0xF0);

    test_bus.write_raw(0x0200, 0x09); // ORA #$0F
    test_bus.write_raw(0x0201, 0x0F);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0xFF);
}

TEST(MOS6502CoreTest, EORImmediate) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0xFF);

    test_bus.write_raw(0x0200, 0x49); // EOR #$FF
    test_bus.write_raw(0x0201, 0xFF);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

TEST(MOS6502CoreTest, CMPImmediate) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x50);

    test_bus.write_raw(0x0200, 0xC9); // CMP #$30
    test_bus.write_raw(0x0201, 0x30);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));  // A >= operand
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Zero));
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Negative));
}

TEST(MOS6502CoreTest, CMPEqual) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x50);

    test_bus.write_raw(0x0200, 0xC9); // CMP #$50
    test_bus.write_raw(0x0201, 0x50);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

// ============================================================
// ADC / SBC Tests
// ============================================================

TEST(MOS6502CoreTest, ADCImmediateNoCarry) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x10);
    test_cpu.set_flag(Flag6502::Carry, false);

    test_bus.write_raw(0x0200, 0x69); // ADC #$20
    test_bus.write_raw(0x0201, 0x20);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x30);
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Carry));
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Overflow));
}

TEST(MOS6502CoreTest, ADCWithCarry) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0xFF);
    test_cpu.set_flag(Flag6502::Carry, false);

    test_bus.write_raw(0x0200, 0x69); // ADC #$01
    test_bus.write_raw(0x0201, 0x01);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

TEST(MOS6502CoreTest, SBCImmediate) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x50);
    test_cpu.set_flag(Flag6502::Carry, true); // Carry set means no borrow

    test_bus.write_raw(0x0200, 0xE9); // SBC #$30
    test_bus.write_raw(0x0201, 0x30);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x20);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));
}

// ============================================================
// Branch Tests
// ============================================================

TEST(MOS6502CoreTest, BNETaken) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_flag(Flag6502::Zero, false); // Z=0 → BNE will branch

    test_bus.write_raw(0x0200, 0xD0); // BNE +$10
    test_bus.write_raw(0x0201, 0x10);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_pc(), 0x0212); // 0x0202 + 0x10
}

TEST(MOS6502CoreTest, BNENotTaken) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_flag(Flag6502::Zero, true); // Z=1 → BNE will NOT branch

    test_bus.write_raw(0x0200, 0xD0); // BNE +$10
    test_bus.write_raw(0x0201, 0x10);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_pc(), 0x0202); // Just past the instruction
}

TEST(MOS6502CoreTest, BEQTaken) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_flag(Flag6502::Zero, true);

    test_bus.write_raw(0x0200, 0xF0); // BEQ -$10 (backwards branch)
    test_bus.write_raw(0x0201, 0xF0); // offset = -16 (two's complement)

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_EQ(test_cpu.get_pc(), 0x01F2); // 0x0202 - 0x10
}

// ============================================================
// Flag Instruction Tests
// ============================================================

TEST(MOS6502CoreTest, FlagSetClear) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);

    // SEC (set carry)
    test_bus.write_raw(0x0200, 0x38);
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry));

    // CLC (clear carry)
    test_cpu.set_pc(0x0201);
    test_bus.write_raw(0x0201, 0x18);
    test_cpu.step(test_bus, cycles);
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Carry));

    // SED (set decimal)
    test_cpu.set_pc(0x0202);
    test_bus.write_raw(0x0202, 0xF8);
    test_cpu.step(test_bus, cycles);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Decimal));

    // CLD (clear decimal)
    test_cpu.set_pc(0x0203);
    test_bus.write_raw(0x0203, 0xD8);
    test_cpu.step(test_bus, cycles);
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Decimal));
}

// ============================================================
// JSR / RTS Test
// ============================================================

TEST(MOS6502CoreTest, JSRRTS) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t reset_cycles = 0;
    test_cpu.reset(test_bus, reset_cycles); // Sets SP to 0xFD
    test_cpu.set_pc(0x0200);

    // JSR $0300
    test_bus.write_raw(0x0200, 0x20);
    test_bus.write_raw(0x0201, 0x00);
    test_bus.write_raw(0x0202, 0x03);

    // Now RTS from $0300
    test_bus.write_raw(0x0300, 0x60); // RTS
    
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // PC should be $0300, return address ($0202) on stack
    EXPECT_EQ(test_cpu.get_pc(), 0x0300);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0xFB); // Pushed 2 bytes

    
    test_cpu.step(test_bus, cycles);
    // Should return to $0203 (address after JSR args)
    EXPECT_EQ(test_cpu.get_pc(), 0x0203);
    EXPECT_EQ(test_cpu.get_reg(Reg6502::SP), 0xFD);
}

// ============================================================
// Shift / Rotate Tests
// ============================================================

TEST(MOS6502CoreTest, ASLAccumulator) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x80);

    test_bus.write_raw(0x0200, 0x0A); // ASL A
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry)); // bit 7 shifted into carry
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

TEST(MOS6502CoreTest, LSRAccumulator) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x01);

    test_bus.write_raw(0x0200, 0x4A); // LSR A
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x00);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry)); // bit 0 shifted into carry
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Zero));
}

TEST(MOS6502CoreTest, ROLAccumulator) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x80);
    test_cpu.set_flag(Flag6502::Carry, true);

    test_bus.write_raw(0x0200, 0x2A); // ROL A
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // 0x80 << 1 = 0x00, carry in = 1 → result = 0x01
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x01);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry)); // bit 7 was set
}

TEST(MOS6502CoreTest, RORAccumulator) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x01);
    test_cpu.set_flag(Flag6502::Carry, true);

    test_bus.write_raw(0x0200, 0x6A); // ROR A
    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // 0x01 >> 1 = 0x00, carry in = 1 → result = 0x80
    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x80);
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Carry)); // bit 0 was set
}

// ============================================================
// BIT Test
// ============================================================

TEST(MOS6502CoreTest, BITZeroPage) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0xFF);

    // BIT $80 — set memory to $C0 (bits 7 and 6 set)
    test_bus.write_raw(0x0200, 0x24); // BIT Zero Page
    test_bus.write_raw(0x0201, 0x80);
    test_bus.write_raw(0x0080, 0xC0);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // A & M = 0xFF & 0xC0 = 0xC0 ≠ 0 → Z=0
    EXPECT_FALSE(test_cpu.get_flag(Flag6502::Zero));
    // bit 6 of M is set → V=1
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Overflow));
    // bit 7 of M is set → N=1
    EXPECT_TRUE(test_cpu.get_flag(Flag6502::Negative));
}

// ============================================================
// Zero Page Wrap Test
// ============================================================

TEST(MOS6502CoreTest, ZeroPageWrap) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0x01);

    // LDA $FF,X — with X=1, effective address = ($FF + 1) & 0xFF = $00
    test_bus.write_raw(0x0200, 0xB5); // LDA Zero Page,X
    test_bus.write_raw(0x0201, 0xFF);
    test_bus.write_raw(0x0000, 0x55); // Value at wrapped address

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_cpu.get_reg(Reg6502::A), 0x55);
}

// ============================================================
// Store Instruction Tests
// ============================================================

TEST(MOS6502CoreTest, STAAbsolute) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::A, 0x42);

    test_bus.write_raw(0x0200, 0x8D); // STA $1234
    test_bus.write_raw(0x0201, 0x34);
    test_bus.write_raw(0x0202, 0x12);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_bus.read(0x1234, cycles), 0x42);
}

TEST(MOS6502CoreTest, STXZeroPage) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::X, 0x99);

    test_bus.write_raw(0x0200, 0x86); // STX $80
    test_bus.write_raw(0x0201, 0x80);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_bus.read(0x0080, cycles), 0x99);
}

TEST(MOS6502CoreTest, STYAbsolute) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x0200);
    test_cpu.set_reg(Reg6502::Y, 0x77);

    test_bus.write_raw(0x0200, 0x8C); // STY $5678
    test_bus.write_raw(0x0201, 0x78);
    test_bus.write_raw(0x0202, 0x56);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    EXPECT_EQ(test_bus.read(0x5678, cycles), 0x77);
}

TEST(MOS6502CoreTest, JmpIndirect_ValidatesCyclesAndSingleFetch) {
    // 1. Setup Memory State
    // Memory:
    // $8000: JMP ($0200) -> Opcode 0x6C, Low $00, High $02
    // $8003: 0xEA (NOP) - If double-fetched, PC reads $8003/$8004 as a second pointer!
    // $8004: 0xEA (NOP)
    // $0200: Low target $34
    // $0201: High target $12 ($1234)

    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_bus.write_raw(0x8000, 0x6C); // JMP Indirect
    test_bus.write_raw(0x8001, 0x00); // Pointer Low ($00)
    test_bus.write_raw(0x8002, 0x02); // Pointer High ($02)
    
    test_bus.write_raw(0x8003, 0xFF); // Dummy data that WOULD corrupt pointer if read
    test_bus.write_raw(0x8004, 0xFF); // Pointer $FFFF would jump to wrong location

    test_bus.write_raw(0x0200, 0x34); // Target PC Low
    test_bus.write_raw(0x0201, 0x12); // Target PC High

    test_cpu.set_pc(0x8000);
    uint32_t cycles = 0;

    // 2. Execute 1 Instruction
    test_cpu.step(test_bus, cycles);

    // 3. Assert Strict Specifications
    EXPECT_EQ(test_cpu.get_pc(), 0x1234) << "PC failed to jump to target $1234";
    EXPECT_EQ(cycles, 5)      << "JMP ($IND) must take exactly 5 cycles (6 for page wrap if applicable)";
}

TEST(MOS6502CoreTest, PLP_StripsBreakFlagAndForcesUnused) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x8000);
    uint32_t cycles = 0;

    // 1. Manually write a byte to the stack with BOTH Break (0x10) and Unused (0x20) set,
    // plus Carry (0x01) to verify normal flags still restore.
    // Stack in 6502 lives at $0100-$01FF. Let's assume SP is at default $FF ($01FF).
    uint8_t stack_val = Flag6502::Break | Flag6502::Unused | Flag6502::Carry; // 0x31
    test_bus.write_raw(0x01FF, stack_val);

    // Set CPU Stack Pointer to point after that byte ($FE)
    test_cpu.set_reg(Reg6502::SP, 0xFE);

    // 2. Execute PLP (Opcode 0x28)
    test_bus.write_raw(0x8000, 0x28); 
    test_cpu.step(test_bus, cycles);

    // 3. Assertions
    uint8_t current_status = test_cpu.get_status();

    EXPECT_TRUE(current_status & Flag6502::Carry) 
        << "PLP failed to restore Carry flag";
    EXPECT_TRUE(current_status & Flag6502::Unused) 
        << "PLP failed to set Unused flag (bit 5)";
    EXPECT_FALSE(current_status & Flag6502::Break) 
        << "CRITICAL: PLP leaked Break flag (bit 4) into internal status register!";
}

TEST(MOS6502CoreTest, RTS_IncrementsPulledAddressByOne) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x8000);
    uint32_t cycles = 0;

    // 1. Simulate a JSR that pushed $8002 (the address of JSR's last byte) onto the stack.
    // 6502 stack pops Low byte first, then High byte.
    // We write High ($80) to $01FE and Low ($02) to $01FD.
    test_bus.write_raw(0x01FE, 0x80); // Return PC High
    test_bus.write_raw(0x01FD, 0x02); // Return PC Low

    // Set SP so the next two pops read $01FD then $01FE
    test_cpu.set_reg(Reg6502::SP, 0xFC);

    // 2. Write RTS opcode (0x60) at $8000
    test_bus.write_raw(0x8000, 0x60);
    
    // Target byte at $8003 (where execution SHOULD resume after +1)
    test_bus.write_raw(0x8003, 0xEA); // NOP

    // 3. Execute RTS
    test_cpu.step(test_bus, cycles);

    // 4. Assertions
    // Without the +1 fix, PC will be $8002. With the fix, PC must be $8003.
    EXPECT_EQ(test_cpu.get_pc(), 0x8003) 
        << "CRITICAL: RTS failed to increment return address by 1! PC is " 
        << std::hex << test_cpu.get_pc() << " instead of $8003";
        
    EXPECT_EQ(cycles, 6) << "RTS must take exactly 6 cycles";
}

TEST(MOS6502CoreTest, RTI_RestoresStatusAndExactPCWithoutIncrement) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    test_cpu.set_pc(0x8000);
    uint32_t cycles = 0;

    // 1. Setup stack with Status, PC Low, PC High
    // Stack order for RTI (pushed P, PC_High, PC_Low -> popped P, PC_Low, PC_High):
    // $01FD: Status byte (0x31 -> Break + Unused + Carry)
    // $01FE: Return PC Low  ($34)
    // $01FF: Return PC High ($12)
    test_bus.write_raw(0x01FD, Flag6502::Break | Flag6502::Unused | Flag6502::Carry);
    test_bus.write_raw(0x01FE, 0x34);
    test_bus.write_raw(0x01FF, 0x12);

    test_cpu.set_reg(Reg6502::SP, 0xFC); // SP points before the pulled bytes

    // 2. Write RTI opcode (0x40)
    test_bus.write_raw(0x8000, 0x40);

    // 3. Step execution
    test_cpu.step(test_bus, cycles);

    // 4. Assertions
    // PC must be EXACTLY $1234 (unlike RTS which would make it $1235)
    EXPECT_EQ(test_cpu.get_pc(), 0x1234) 
        << "RTI failed! PC should be $1234, got " << std::hex << test_cpu.get_pc();

    uint8_t status = test_cpu.get_status();
    EXPECT_TRUE(status & Flag6502::Carry)  << "RTI failed to restore Carry flag";
    EXPECT_TRUE(status & Flag6502::Unused) << "RTI failed to set Unused flag (bit 5)";
    EXPECT_FALSE(status & Flag6502::Break) << "RTI leaked Break flag (bit 4)";
    
    EXPECT_EQ(cycles, 6) << "RTI must take exactly 6 cycles";
}

TEST(MOS6502CoreTest, BRK_PushesPWithBreakSet_MasksIRQ_ReadsFFFEVector) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t setup_cycles = 0;

    // 1. Setup IRQ/BRK Vector at $FFFE -> $9000
    test_bus.write(0xFFFE, 0x00, setup_cycles); // Vector Low
    test_bus.write(0xFFFF, 0x90, setup_cycles); // Vector High

    // Place BRK at $8000. $8001 is the padding byte.
    // Pushed return address on stack MUST be $8002 ($8000 + 2).
    test_bus.write(0x8000, 0x00, setup_cycles); // BRK opcode
    test_bus.write(0x8001, 0xAA, setup_cycles); // Padding / signature byte

    test_cpu.set_pc(0x8000);
    test_cpu.set_reg(Reg6502::SP, 0xFF); // Stack lives at $0100-$01FF

    uint32_t cycles = 0;

    // 2. Execute BRK instruction
    test_cpu.step(test_bus, cycles);

    // 3. Assertions
    // Target PC loaded from $FFFE/$FFFF
    EXPECT_EQ(test_cpu.get_pc(), 0x9000)
        << "BRK failed to load PC from vector $FFFE/$FFFF";

    // Internal status register must set Interrupt Disable (I)
    EXPECT_TRUE(test_cpu.get_status() & Flag6502::Interrupt)
        << "BRK failed to set Interrupt Disable flag (I)";

    // Verify Stack Contents using isolated cycle tracking
    uint32_t inspect_cycles = 0;
    
    // Stack pushes High PC, Low PC, then Status byte (P)
    EXPECT_EQ(test_bus.read(0x01FF, inspect_cycles), 0x80)
        << "Stack PC High incorrect (Expected 0x80)";
    EXPECT_EQ(test_bus.read(0x01FE, inspect_cycles), 0x02)
        << "Stack PC Low incorrect (BRK must push PC + 2)";

    uint8_t pushed_p = test_bus.read(0x01FD, inspect_cycles);
    EXPECT_TRUE(pushed_p & Flag6502::Break)
        << "BRK failed to set Break flag (bit 4) on stack";
    EXPECT_TRUE(pushed_p & Flag6502::Unused)
        << "BRK failed to set Unused flag (bit 5) on stack";

    // Strict timing verification
    EXPECT_EQ(cycles, 7)
        << "BRK must take exactly 7 cycles";
}

TEST(MOS6502CoreTest, BRK_FollowedBy_RTI_ResumesAtCorrectAddress) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t dummy = 0;

    // 1. Set Interrupt Vector ($FFFE/$FFFF) -> $9000
    test_bus.write(0xFFFE, 0x00, dummy);
    test_bus.write(0xFFFF, 0x90, dummy);

    // 2. Main code at $8000:
    // $8000: BRK (0x00)
    // $8001: Signature byte (0xAA)
    // $8002: NOP (0xEA) <- RTI MUST return here!
    test_bus.write(0x8000, 0x00, dummy); 
    test_bus.write(0x8001, 0xAA, dummy); 
    test_bus.write(0x8002, 0xEA, dummy); 

    // 3. ISR code at $9000:
    // $9000: RTI (0x40)
    test_bus.write(0x9000, 0x40, dummy);

    test_cpu.set_pc(0x8000);
    test_cpu.set_reg(Reg6502::SP, 0xFF);

    uint32_t brk_cycles = 0;
    test_cpu.step(test_bus, brk_cycles);

    // Assert BRK landed in ISR
    ASSERT_EQ(test_cpu.get_pc(), 0x9000);
    ASSERT_EQ(brk_cycles, 7);

    uint32_t rti_cycles = 0;
    test_cpu.step(test_bus, rti_cycles);

    // Assert RTI returned to $8002 (skipping signature byte $8001)
    EXPECT_EQ(test_cpu.get_pc(), 0x8002) 
        << "BRK/RTI loop failed! Expected PC $8002, got " << std::hex << test_cpu.get_pc();
    
    EXPECT_EQ(rti_cycles, 6) << "RTI must take exactly 6 cycles";
}

TEST(MOS6502CoreTest, JMP_Indirect_PageBoundaryWrapBug) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t dummy = 0;

    // Set up vector split across page boundary:
    // $02FF holds low byte 0x34
    // $0200 holds high byte 0x12 (wrapped around on page $02)
    // $0300 holds incorrect high byte 0x99 (if page boundary wasn't bugged)
    test_bus.write(0x02FF, 0x34, dummy);
    test_bus.write(0x0200, 0x12, dummy);
    test_bus.write(0x0300, 0x99, dummy);

    // Write JMP ($02FF) opcode 0x6C at $8000
    test_bus.write(0x8000, 0x6C, dummy); // JMP (ind)
    test_bus.write(0x8001, 0xFF, dummy); // Low addr byte
    test_bus.write(0x8002, 0x02, dummy); // High addr byte

    test_cpu.set_pc(0x8000);
    uint32_t cycles = 0;

    test_cpu.step(test_bus, cycles);

    // PC should land at $1234 (from $02FF and $0200), NOT $9934 (from $02FF and $0300)
    EXPECT_EQ(test_cpu.get_pc(), 0x1234)
        << "JMP ($xxFF) failed hardware page wrap! PC went to " << std::hex << test_cpu.get_pc();
    
    EXPECT_EQ(cycles, 5) << "JMP indirect must take exactly 5 cycles";
}

// ============================================================
// IRQ / NMI Interrupt Handling Tests
// ============================================================

TEST(MOS6502CoreTest, IRQ_RespectsInterruptDisableFlag) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t dummy = 0;

    // 1. Setup $8000 with load instruction
    test_bus.write(0x8000, 0xA9, dummy);
    test_bus.write(0x8001, 0x05, dummy);

    // 2. Setup IRQ Vector at $FFFE/$FFFF -> $8500
    test_bus.write(0xFFFE, 0x00, dummy);
    test_bus.write(0xFFFF, 0x85, dummy);

    test_cpu.set_pc(0x8000);
    
    // 2. Set Interrupt Disable (I) flag to true
    test_cpu.set_flag(Flag6502::Interrupt, true);

    // 3. Trigger IRQ line assert (assuming your CPUCore has an external pin/line trigger)
    test_bus.set_irq_line(true); 

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // 4. Assertions: IRQ should be ignored due to I=1; PC should execute normal instruction at $8000
    EXPECT_NE(test_cpu.get_pc(), 0x8500)
        << "CRITICAL: IRQ fired while Interrupt Disable (I) flag was set!";
}

TEST(MOS6502CoreTest, IRQ_ExecutesWhenEnabled_PushesStateAndVectors) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t dummy = 0;

    // 1. Setup $8000 with load instruction
    test_bus.write(0x8000, 0xA9, dummy);
    test_bus.write(0x8001, 0x05, dummy);

    // 2. Setup IRQ Vector at $FFFE/$FFFF -> $8500
    test_bus.write(0xFFFE, 0x00, dummy);
    test_bus.write(0xFFFF, 0x85, dummy);

    test_cpu.set_pc(0x8000);
    test_cpu.set_reg(Reg6502::SP, 0xFF);
    
    // 3. Clear Interrupt Disable (I) flag so IRQ is accepted
    test_cpu.set_flag(Flag6502::Interrupt, false);
    test_cpu.set_flag(Flag6502::Break, false); // Explicitly clear B for hardware IRQ

    // 4. Trigger IRQ line
    test_bus.set_irq_line(true);

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // 5. Assertions
    EXPECT_EQ(test_cpu.get_pc(), 0x8500)
        << "IRQ failed to vector to $8500";
    
    // Check that Break flag is NOT set on the stack for a hardware IRQ (unlike BRK)
    uint32_t inspect_cycles = 0;
    uint8_t pushed_p = test_bus.read(0x01FD, inspect_cycles);
    EXPECT_FALSE(pushed_p & Flag6502::Break)
        << "Hardware IRQ incorrectly set Break flag (bit 4) on stack";
    EXPECT_TRUE(test_cpu.get_status() & Flag6502::Interrupt)
        << "IRQ failed to set internal Interrupt Disable flag";
    
    EXPECT_EQ(cycles, 7) << "IRQ hardware interrupt sequence must take 7 cycles";
}

TEST(MOS6502CoreTest, NMI_IgnoresInterruptDisableFlag_VectorsToFFFA) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    uint32_t dummy = 0;

    // 1. Setup $8000 with load instruction
    test_bus.write(0x8000, 0xA9, dummy);
    test_bus.write(0x8001, 0x05, dummy);

    // 2. Setup NMI Vector at $FFFA/$FFFB -> $8800
    test_bus.write(0xFFFA, 0x00, dummy);
    test_bus.write(0xFFFB, 0x88, dummy);

    test_cpu.set_pc(0x8000);
    test_cpu.set_reg(Reg6502::SP, 0xFF);

    // 3. Set Interrupt Disable (I) flag — NMI must bypass this completely
    test_cpu.set_flag(Flag6502::Interrupt, true);

    // 4. Trigger NMI edge/line
    test_bus.trigger_nmi();

    uint32_t cycles = 0;
    test_cpu.step(test_bus, cycles);

    // 5. Assertions
    EXPECT_EQ(test_cpu.get_pc(), 0x8800)
        << "NMI failed to vector to $8800";

    // NMI also disables further regular interrupts
    EXPECT_TRUE(test_cpu.get_status() & Flag6502::Interrupt)
        << "NMI failed to set Interrupt Disable flag";

    EXPECT_EQ(cycles, 7) << "NMI interrupt sequence must take 7 cycles";
}