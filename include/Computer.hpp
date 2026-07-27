#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

namespace hws {

template <typename Config, typename BusType, typename CPUType>
class Computer {
public:
    using Addr = typename Config::AddrType;
    using Data = typename Config::DataType;

private:
    BusType bus_;
    CPUType cpu_;
    uint32_t cycles_ = 0;

public:
    Computer() {
        cpu_.reset(bus_, cycles_);
    }

    // Load raw machine code binary directly into the bus memory map
    bool load_binary(const std::string& filepath, Addr load_address) {
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            for (size_t i = 0; i < buffer.size(); ++i) {
                bus_.write_raw(load_address + static_cast<Addr>(i), buffer[i]);
            }
            return true;
        }
        return false;
    }

    // Explicitly set the 6502 Reset Vector ($FFFC-$FFFD)
    void set_reset_vector(Addr vector) {
        uint32_t dummy_cycles = 0;
        bus_.write(0xFFFC, vector & 0xFF, dummy_cycles);
        bus_.write(0xFFFD, (vector >> 8) & 0xFF, dummy_cycles);
        cpu_.reset(bus_, cycles_);
    }

    // Execute a single CPU step
    void step() {
        cpu_.step(bus_, cycles_);
    }

    // Run execution loop up to a maximum cycle count (0 = infinite)
    void run(uint64_t max_cycles = 0) {
        uint64_t executed_cycles = 0;
        while (max_cycles == 0 || executed_cycles < max_cycles) {
            uint32_t prev_cycles = cycles_;
            cpu_.step(bus_, cycles_);
            executed_cycles += (cycles_ - prev_cycles);
        }
    }

    BusType& get_bus() { return bus_; }
    CPUType& get_cpu() { return cpu_; }
    uint32_t get_cycles() const { return cycles_; }
};

} // namespace hws