#pragma once
#include "PlatformConfig.hpp"
#include <array>
#include <iostream>

// Pure, zero-overhead compile-time test bus matching your configuration types
template <typename Config>
class FlatMemoryBus {
public:
    using Addr = typename Config::AddrType;
    using Data = typename Config::DataType;

private:
    std::array<Data, 65536> memory{};
    bool irq_line_ = false;
    bool nmi_triggered_ = false;
    bool exit_requested_ = false;

public:
    FlatMemoryBus() { memory.fill(0); }

    bool is_exit_requested() const { return exit_requested_; }

    inline Data read_raw(Addr address) {
        return memory[address];
    }

    inline Data read(Addr address, uint32_t& cycle_accumulator) {
        cycle_accumulator += 1;
        return memory[address];
    }

    inline void write(Addr address, Data data, uint32_t& cycle_accumulator) {
        cycle_accumulator += 1;

        // MMIO: Route writes to 0xF001 directly to terminal output
        if (address == 0xF001) {
            // Existing MMIO serial output (e.g., printing 'H', 'I', '\n')
            std::cout << static_cast<char>(data);
            std::cout.flush();
        } 
        else if (address == 0xF002) {
            // Dedicated test exit trap
            exit_requested_ = true;
        }
        else {
            // Standard RAM / memory write
            memory[address] = data;
        }
    }

    inline void write_raw(Addr address, Data data) {
        memory[address] = data;
    }

    // Zero-cycle, non-mutating inspect for logging/debugging
    inline Data peek(Addr address) const {
        return memory[address];
    }

    // Direct hardware line controls for your test suite
    inline void set_irq_line(bool state) {
        irq_line_ = state;
    }

    inline bool get_irq_line() const {
        return irq_line_;
    }

    inline void trigger_nmi() {
        nmi_triggered_ = true;
    }

    inline bool consume_nmi() {
        bool state = nmi_triggered_;
        nmi_triggered_ = false;
        return state;
    }
};