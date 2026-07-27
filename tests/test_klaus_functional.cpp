#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include "CPUCore.hpp"
#include "SystemBus.hpp"
#include "PlatformConfig.hpp"


TEST(MOS6502FunctionalTest, KlausDormannSuite) {
    FlatMemoryBus<KIM1_Config> test_bus;
    CPUCore<KIM1_Config> test_cpu;

    // 1. Load the binary file into memory at $000A
    std::ifstream file("6502_functional_test.bin", std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "Failed to open 6502_functional_test.bin!";

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
    file.close();

    // Write binary bytes directly starting at 0x0000
    uint32_t dummy = 0;
    for (size_t i = 0; i < buffer.size(); ++i) {
        test_bus.write(static_cast<uint16_t>(0x0000 + i), buffer[i], dummy);
    }

    // 2. Explicitly patch the Reset Vector to $0400 if the bin file didn't include it
    test_bus.write_raw(0xFFFC, 0x00); // Low byte
    test_bus.write_raw(0xFFFD, 0x04); // High byte ($0400)

    uint32_t reset_cycles = 0;
    // 1. Trigger hardware reset sequence
    test_cpu.reset(test_bus, reset_cycles); // Reads $FFFC/D, sets PC = $0400, initial SP, & flags

    // 1. Execution Loop
    uint32_t total_cycles = 0;
    constexpr uint16_t SUCCESS_TARGET_PC = 0x3469;
    constexpr uint64_t MAX_CYCLES = 100'000'000;

    // Replace the rolling history ring buffer with a stuck-loop counter
    uint16_t last_pc = 0x0000;
    uint32_t consecutive_count = 0;
    constexpr uint32_t MAX_CONSECUTIVE = 100; // Allows short multi-instruction loops to pass safely

    while (total_cycles < MAX_CYCLES) {
        uint16_t current_pc = test_cpu.get_pc();

        // Check Pass Condition
        if (current_pc == SUCCESS_TARGET_PC) {
            SUCCEED() << "Klaus Dormann Suite PASSED at cycle " << total_cycles;
            std::cout << "Klaus Dormann Suite PASSED at cycle " << total_cycles << std::endl;
            return;
        }

        // Check for an actual infinite hang (e.g., JMP $ stuck loops)
        if (current_pc == last_pc && current_pc != 0x0000) {
            consecutive_count++;
            if (consecutive_count >= MAX_CONSECUTIVE) {
                FAIL() << "Klaus Test Trapped in loop at PC = $" 
                       << std::hex << std::uppercase << current_pc 
                       << " (Cycles: " << std::dec << total_cycles << ")";
                return;
            }
        } else {
            last_pc = current_pc;
            consecutive_count = 0;
        }

        test_cpu.step(test_bus, total_cycles);
    }

    FAIL() << "Klaus Test timed out! Final PC = $" 
           << std::hex << std::uppercase << test_cpu.get_pc();
}