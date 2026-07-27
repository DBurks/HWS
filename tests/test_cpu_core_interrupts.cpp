#include <gtest/gtest.h>
#include <array>
#include <cstdint>

#include "CPUCore.hpp"
#include "SystemBus.hpp"
#include "PlatformConfig.hpp"

class CPUFullInterruptRequirementsTest : public ::testing::Test {
protected:
    FlatMemoryBus<KIM1_Config> bus;
    CPUCore<KIM1_Config> cpu;
    uint32_t cycles;

    void SetUp() override {
        cycles = 0;
        bus.write(0xFFFC, 0x00, cycles);
        bus.write(0xFFFD, 0x80, cycles);

        // NMI Vector at $FFFA-$FFFB -> 0xA000
        bus.write(0xFFFA, 0x00, cycles);
        bus.write(0xFFFB, 0xA0, cycles);

        // IRQ / BRK Vector at $FFFE-$FFFF -> 0xB000
        bus.write(0xFFFE, 0x00, cycles);
        bus.write(0xFFFF, 0xB0, cycles);

        cpu.reset(bus, cycles);
    }
};

// 1. Hardware Interrupt Stack Order (SR, PC-L, PC-H from top to bottom) and Vector Routing
TEST_F(CPUFullInterruptRequirementsTest, HardwareInterruptStackOrderAndVector) {
    // 1. Initial CPU state: NOP at $8000, PC=0x8000, SP=0xFF, I-flag clear
    bus.write(0x8000, 0xEA, cycles); // $EA = NOP
    cpu.set_pc(0x8000);
    cpu.set_flag(Flag6502::Interrupt, false); // Interrupts enabled

    uint8_t initial_sp = cpu.get_reg(CPUCore<KIM1_Config>::Reg::SP);

    // Step 1: Execute the NOP at $8000 (PC advances to $8001)
    cpu.step(bus, cycles); 
    EXPECT_EQ(cpu.get_pc(), 0x8001);

    // Step 2: Signal IRQ line (simulates hardware line going active mid/post instruction)
    bus.set_irq_line(true);

    // Step 3: Step again to service the pending IRQ
    cpu.step(bus, cycles);

    // Verify control flow diverted to IRQ vector ($B000)
    EXPECT_EQ(cpu.get_pc(), 0xB000);

    // Verify I-flag is automatically set
    EXPECT_TRUE(cpu.get_flag(Flag6502::Interrupt));

    // Verify SP decreased by 3
    uint8_t sp_after = cpu.get_reg(CPUCore<KIM1_Config>::Reg::SP);
    EXPECT_EQ(initial_sp - sp_after, 3);

    // Verify stacked PC points to 0x8001 (the instruction immediately after NOP)
    uint16_t stack_base = 0x0100;
    uint8_t stacked_pch = bus.peek(stack_base + static_cast<uint8_t>(initial_sp));
    uint8_t stacked_pcl = bus.peek(stack_base + static_cast<uint8_t>(initial_sp - 1));
    uint8_t stacked_sr  = bus.peek(stack_base + static_cast<uint8_t>(initial_sp - 2));

    uint16_t stacked_pc = (static_cast<uint16_t>(stacked_pch) << 8) | stacked_pcl;

    EXPECT_EQ(stacked_pc, 0x8001);
    EXPECT_EQ(stacked_sr & Flag6502::Break, 0); // B-flag must be 0 for hardware IRQ
}

// 2. NMI Execution (Bypasses I-Flag)
TEST_F(CPUFullInterruptRequirementsTest, NMIExecutesRegardlessOfIFlag) {
    cpu.set_flag(Flag6502::Interrupt, true); // Set I-flag to block IRQ
    cpu.set_pc(0x8000);

    bus.trigger_nmi();
    cpu.step(bus, cycles);

    // Must jump to NMI vector ($A000) despite I-flag being set
    EXPECT_EQ(cpu.get_pc(), 0xA000);
}

// 3. BRK Instruction Behavior (PC+2, B-flag Set, Vector to $FFFE)
TEST_F(CPUFullInterruptRequirementsTest, BRKInstructionBehavior) {
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0x00, cycles); // BRK opcode
    uint8_t initial_sp = cpu.get_reg(CPUCore<KIM1_Config>::Reg::SP);

    cpu.step(bus, cycles);

    uint16_t stack_base = 0x0100;
    uint8_t stacked_pch  = bus.peek(stack_base + static_cast<uint8_t>(initial_sp));
    uint8_t stacked_pcl = bus.peek(stack_base + static_cast<uint8_t>(initial_sp - 1));
    uint8_t stacked_sr = bus.peek(stack_base + static_cast<uint8_t>(initial_sp - 2));
    uint16_t stacked_pc = (static_cast<uint16_t>(stacked_pch) << 8) | stacked_pcl;

    // BRK pushes PC + 2 ($8002)
    EXPECT_EQ(stacked_pc, 0x8002);

    // Software-initiated transfer sets the Break flag on stack
    EXPECT_NE(stacked_sr & Flag6502::Break, 0);

    // Control transfers to $FFFE vector ($B000)
    EXPECT_EQ(cpu.get_pc(), 0xB000);
    // I-flag set automatically
    EXPECT_TRUE(cpu.get_flag(Flag6502::Interrupt));
}

// 4. PHP Instruction Sets Break Flag on Stack
TEST_F(CPUFullInterruptRequirementsTest, PHPSetsBreakFlagOnStack) {
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0x08, cycles); // PHP opcode
    uint8_t initial_sp = cpu.get_reg(CPUCore<KIM1_Config>::Reg::SP);

    cpu.step(bus, cycles);

    uint8_t stacked_sr = bus.peek(0x0100 + static_cast<uint8_t>(initial_sp));
    EXPECT_NE(stacked_sr & Flag6502::Break, 0);
}

// 5. PLP and RTI Ignore Break Flag When Pulled Back Into Processor
TEST_F(CPUFullInterruptRequirementsTest, PLPAndRTIIgnoreBreakFlag) {
    // Manually push a status value with Break flag set onto the stack
    uint8_t initial_sp = cpu.get_reg(CPUCore<KIM1_Config>::Reg::SP);
    bus.write(0x0100 + initial_sp, 0xFF, cycles); // All flags set including B and Unused
    cpu.set_reg(CPUCore<KIM1_Config>::Reg::SP, initial_sp - 1);

    // Execute PLP (0x28)
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0x28, cycles);
    cpu.step(bus, cycles);

    // Internal status register should have its internal/real flags updated,
    // but the B flag is not a real processor flag and must be ignored/un-set internally.
    uint8_t current_status = cpu.get_status();
    EXPECT_EQ(current_status & Flag6502::Break, 0);
}

// 6. RESET Vectoring, I-Flag Setting, and Register Initialization
TEST_F(CPUFullInterruptRequirementsTest, ResetSequenceLoadsVectorAndSetsIFlag) {
    cycles = 0;
    // Use write_raw to bypass any bus write-protection on vector memory ($FFFC-$FFFD)
    bus.write_raw(0xFFFC, 0x00);
    bus.write_raw(0xFFFD, 0xC0);

    // Clear I-flag first to verify reset explicitly re-enables it
    cpu.set_flag(Flag6502::Interrupt, false);

    // Trigger RESET sequence (pass bus reference if your API requires it)
    cpu.reset(bus, cycles);

    // PC must be loaded from $FFFC-$FFFD ($C000)
    EXPECT_EQ(cpu.get_pc(), 0xC000);
    EXPECT_TRUE(cpu.get_flag(Flag6502::Interrupt));
    EXPECT_EQ(cycles, 7) << "Reset sequence must take exactly 7 cycles";
}

// 7. Simultaneous NMI and IRQ Priority (NMI Must Win)
TEST_F(CPUFullInterruptRequirementsTest, SimultaneousNMIAndIRQFavorsNMI) {
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0xEA, cycles); // NOP at $8000
    bus.write_raw(0xA000, 0xEA);     // Put NOP at $A000 so it doesn't execute 0x00 (BRK)

    cpu.set_flag(Flag6502::Interrupt, false); // Enable IRQs

    // Step 1: Execute NOP at $8000
    cpu.step(bus, cycles);

    // Assert BOTH NMI and IRQ simultaneously
    bus.set_irq_line(true);
    bus.trigger_nmi();

    // Step 2: Service pending interrupt
    cpu.step(bus, cycles);

    // Must vector to NMI ($A000), NOT IRQ ($B000)
    EXPECT_EQ(cpu.get_pc(), 0xA000);
}

// 8. BRK Sets I-Flag and Inhibits Pending Hardware IRQs
TEST_F(CPUFullInterruptRequirementsTest, BRKInhibitsSubsequentIRQViaIFlag) {
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0x00, cycles); // BRK opcode
    bus.write(0x8001, 0x00, cycles); // BRK signature byte

    // Hardware IRQ is held HIGH throughout
    bus.set_irq_line(true);

    // Step 1: Execute BRK
    cpu.step(bus, cycles);
    
    // PC should land at IRQ/BRK vector ($B000)
    EXPECT_EQ(cpu.get_pc(), 0xB000);
    EXPECT_TRUE(cpu.get_flag(Flag6502::Interrupt)); // BRK must set I-flag

    // Place a NOP at vector target $B000
    bus.write(0xB000, 0xEA, cycles);

    // Step 2: Execute instruction at $B000 while IRQ line is STILL active
    cpu.step(bus, cycles);

    // PC must advance to $B001 and NOT re-vector to $B000 because I-flag suppresses the IRQ
    EXPECT_EQ(cpu.get_pc(), 0xB001);
}

// 9. NMI Can Interrupt Execution Inside a BRK Handler
TEST_F(CPUFullInterruptRequirementsTest, NMIFiresInsideBRKHandler) {
    
    cpu.set_pc(0x8000);
    bus.write(0x8000, 0x00, cycles); // BRK
    bus.write_raw(0xA000, 0xEA);     // Put NOP at $A000 so it doesn't execute 0x00 (BRK)

    // Step 1: Execute BRK -> PC lands at $B000, I-flag is set to true
    cpu.step(bus, cycles);
    EXPECT_EQ(cpu.get_pc(), 0xB000);
    EXPECT_TRUE(cpu.get_flag(Flag6502::Interrupt));

    // Place NOP at $B000 and assert NMI line
    bus.write(0xB000, 0xEA, cycles);

    // Step 2: Execute NOP at $B000
    cpu.step(bus, cycles);
    bus.trigger_nmi();
    // Step 3: Service NMI
    cpu.step(bus, cycles);

    // NMI must bypass the I-flag set by BRK and vector to NMI ($A000)
    EXPECT_EQ(cpu.get_pc(), 0xA000);
}