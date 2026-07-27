# System Patterns & Architecture

## Technical Stack
- **Language:** ISO C++20 Standard.
- **Build System:** Target-Based Modern CMake (version >= 3.24) utilizing `CMakePresets.json` (Ninja generator).
- **Validation Framework:** GoogleTest (fetched dynamically).

## Design Architecture Rules
1. **Compile-Time Polymorphism:** Avoid virtual function lookup tables (`vtables`) inside the high-frequency CPU execution path. Use **Policy-Based Design** via C++ templates.
2. **Policy Configuration Toggles:** System widths (address types, data types, register enums) are determined strictly by a static traits tag passed into the structures (e.g., `CPUCore<KIM1_Config>`).
3. **Register Storage Isolation:** Represent processor state registers as a generalized contiguous array (Register File) indexed cleanly via explicit type enums provided by the active platform configuration policy.
4. **Time Quantization:** Instruction steps update a transaction-level cycle budget accumulator. Real-world synchronization occurs predictably at 1 ms absolute intervals via modern `steady_clock` monotonic anchors.