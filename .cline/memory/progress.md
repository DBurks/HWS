# Progress Log

## Completed Milestones
- [x] Initialized workspace root as `HWS`.
- [x] Implemented modern Target-Based CMake build parameters.
- [x] Standardized compilation via `CMakePresets.json` using the Ninja toolchain.
- [x] Integrated GoogleTest harness suite infrastructure.
- [x] Implemented abstract template structures for `SystemBus` and basic `FlatMemoryBus`.
- [x] Proved end-to-end scaffolding validation via explicit `NOP` cycle-weight testing metrics.
- [x] Phase 2: Added AddressingMode enum (13 modes), Instruction enum (56 mnemonics), OpcodeInfo struct, `addressing_mode_bytes()` helper, and full 16×16 constexpr opcode table (151 opcodes) matching masswerk reference.
- [x] Phase 2: Refactored CPUCore::step() to use functor-based opcode handler jump table (256-entry `kOpcodeHandlers` array via `build_handler_table()`). Added all addressing mode fetchers (ZP, ZPX, ZPY, ABS, ABX, ABY, IND, IZX, IZY, REL, IMM). Implemented handlers for: NOP, BRK, LDA/LDX/LDY (all modes), STA/STX/STY, TAX/TAY/TXA/TYA/TSX/TXS, PHA/PHP/PLA/PLP, INX/INY/DEX/DEY, INC/DEC, AND/ORA/EOR, CMP/CPX/CPY, ADC/SBC, ASL/LSR/ROL/ROR (acc+mem), BIT, JMP/JSR/RTS/RTI, CLC/SEC/CLD/SED/CLI/SEI/CLV, all 8 branches. Generic dispatch fallback for unregistered opcodes. Cycle-accuracy via start/end delta tracking.
- [x] Identified and documented 6502 core hardware edge-cases & test gap requirements (Indexed Indirect page wrapping, Indirect Indexed page-cross cycle penalties, Branch page-crossing timing penalties, BCD Decimal Mode, and Memory RMW cycle rules).
- [x] Verified and locked down core stack & control flow edge cases via explicit unit tests (44 passing tests total):
  - Strip Break flag (bit 4) and force Unused flag (bit 5) high on status restoration during `PLP` and `RTI`.
  - Proper return address offset increment (+1) on `RTS`.
  - Exact 7-cycle `BRK` vector fetch ($FFFE/$FFFF), stack frame layout ($PC+2$), `I` flag masking, and seamless `BRK` $\rightarrow$ `RTI` control flow roundtrips.
  - Page boundary wraparound hardware defect handling on `JMP ($xxFF)`.
- [x] **Klaus Dormann 6502 Functional Test Suite PASSES** — all 9,629,136 cycles execute correctly, validating full opcode correctness across the entire 151-opcode instruction set. This is the gold standard for 6502 emulation correctness.
- [x] Expanded test coverage to 57 tests total, including the `CPUFullInterruptRequirementsTest` suite (9 tests) covering:
  - Hardware IRQ stack order and vector routing
  - NMI execution bypassing I-flag
  - BRK instruction behavior (PC+2, B-flag set, vector to $FFFE)
  - PHP sets Break flag on stack
  - PLP and RTI ignore Break flag when pulling status
  - RESET vectoring, I-flag setting, and register initialization
  - Simultaneous NMI and IRQ priority (NMI wins)
  - BRK inhibits subsequent hardware IRQs via I-flag
  - NMI fires inside a BRK handler

## Active Issues & Bugs under Tracking
- [ ] **Bug: Interrupt handling model in `step()`** — 4 failing tests (53/57 passing).
  - Interrupts polled AFTER instruction execution; correct behavior is to sample interrupts BEFORE opcode fetch.
  - `signal_nmi()` does not set I-flag (passes `set_i_flag = false`); correct 6502 behavior requires NMI to set I-flag.
  - Tests requiring fixes:
    1. `MOS6502CoreTest.IRQ_ExecutesWhenEnabled_PushesStateAndVectors` — cycle count off
    2. `MOS6502CoreTest.NMI_IgnoresInterruptDisableFlag_VectorsToFFFA` — cycle count + I-flag not set by NMI
    3. `CPUFullInterruptRequirementsTest.SimultaneousNMIAndIRQFavorsNMI` — NMI doesn't set I-flag, IRQ overrides
    4. `CPUFullInterruptRequirementsTest.NMIFiresInsideBRKHandler` — cascading from NMI I-flag issue
- [ ] Bug: Indirect Indexed `(zp),Y` page boundary crossing must dynamically add +1 cycle penalty to base execution time.
- [ ] Bug: Relative Branching cycle timing requires proper delta tracking (2 base, +1 if taken, +1 additional if target crosses page boundary).
- [ ] Bug: Read-Modify-Write (RMW) instructions (`INC`, `DEC`, `ASL`, `LSR`, `ROL`, `ROR`) operating directly on memory require exact cycle modeling (+1 always on indexed write/RMW).
- [ ] Feature Gap: Full BCD (Decimal Mode) `ADC`/`SBC` arithmetic and flag setting semantics need explicit unit test validation.

## Upcoming Pipeline Steps
- [ ] **Phase 2.5: Fix interrupt handling model**
  - Move interrupt polling to START of `step()` before opcode fetch
  - Fix `signal_nmi()` to set I-flag
  - Adjust `CPUFullInterruptRequirementsTest` expectations for new polling model
  - Verify Klaus suite still passes after changes
- [ ] Phase 2: Write comprehensive TYPED_TEST suites for remaining addressing mode handlers and edge-case timing penalties.
- [ ] Phase 3: Out-of-band Debugging CLI Engine (tracepoints and watchpoints).
- [ ] Phase 3.5: Standalone binary loading and execution runner in `src/`.
- [ ] Phase 4: Systematic Instruction Decoding mapping (151 Opcodes) — complete, verified by Klaus suite.
- [ ] Phase 5: Real-time 1 ms anchor throttling loop configuration.
- [ ] Phase 6: Tooling & Automation Layer
  - [ ] Implement an inline 2-pass 6502 Assembler (`tools/assembler`)
  - [ ] Implement 6502 compiler/interpreter (`tools/interpreter`)
  - [ ] Standardize string-to-bytecode text ingestion pipelines for test fixtures
  - [ ] Map historical KIM-1 ROM monitor character I/O vectors ($1A7A / $1E5A) for TTY terminal interaction

## Future Stretch Goals (Post-Core)
- [ ] Custom 6502 assembler/compiler targeting BASIC, FORTRAN, and C++ dialects (parallel development)
- [ ] Multi-architecture support feasibility — design abstract decode/execute interface for new instruction sets
- [ ] Instruction pipelining and cache emulation models
- [ ] Sparse guest-to-host memory mapping via hierarchical radix trees with direct pointers for dense regions
- [ ] Avionics/marine communication protocol simulation (MIL-STD-1553, ARINC 429, CAN, Ethernet, RS422, USB) using structured transactions and asynchronous worker nodes
- [ ] Load and run real 6502 ROM images (e.g., Microsoft BASIC, KIM-1 monitor) on the simulator
- [ ] Implement serial TTY terminal I/O so the simulated CPU can read keystrokes and print text interactively
- [ ] Host-side BASIC-to-6502 compiler that translates BASIC source into 6502 machine code for the simulator
- [ ] Full platform emulation profiles (KIM-1, Commodore 64, Apple II, NES) selectable via Config policy tags
- [ ] Cycle-exact peripheral simulation (6530 RIOT timers, VIC-II video, SID audio)