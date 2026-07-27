#include "PlatformConfig.hpp" // Contains KIM1_Config
#include "SystemBus.hpp"
#include "CPUCore.hpp"
#include "Computer.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: hws_sim <path_to_binary> [load_address_hex]\n";
        return 1;
    }

    std::string binary_path = argv[1];
    
    // Default to KIM1_Config::ResetVector (0x0400) if no explicit load address is passed
    uint16_t load_address = KIM1_Config::ResetVector;
    if (argc >= 3) {
        load_address = static_cast<uint16_t>(std::stoul(argv[2], nullptr, 16));
    }

    // Instantiate using your KIM-1 configuration pipeline
    using MyComputer = hws::Computer<KIM1_Config, FlatMemoryBus<KIM1_Config>, CPUCore<KIM1_Config>>;
    
    MyComputer computer;
    std::cout << "6502 Virtual Machine initialized with KIM1_Config.\n";
    
    // Load the assembled machine code binary into memory
    std::cout << "Loading binary: " << binary_path << " at address 0x" 
              << std::hex << load_address << std::dec << "\n";
              
    if (!computer.load_binary(binary_path, load_address)) {
        std::cerr << "Error: Failed to open or read binary file: " << binary_path << "\n";
        return 1;
    }

    // Set the CPU reset vector ($FFFC-$FFFD) to jump straight to our loaded binary
    computer.set_reset_vector(load_address);

    std::cout << "Starting processor execution...\n\n" << std::flush;

    // Run the simulation loop (pass a max cycle count if you want a guard, e.g., 10000)
    computer.run(0); 

    std::cout << "\nExecution completed. Total cycles executed: " << computer.get_cycles() << "\n";
    return 0;
}