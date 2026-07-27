# Active Context

## Current Status
Phase 2 core CPU pipeline is substantially complete with 53/57 tests passing. `CPUCore::step()` uses functor-based handler dispatch via a 256-entry member function pointer jump table (`build_handler_table<BusType>()`). Core instruction edge cases including status flag stripping (`PLP`/`RTI`), hardware vector fetches (`BRK`), return address correction (`RTS`), and page-wrapping indirect jumps (`JMP ($xxFF)`) are fully verified.

**Klaus Dormann's 6502 Functional Test Suite PASSES** — all 9,629,136 cycles execute correctly, validating full opcode correctness across the entire instruction set.

## Architecture & Design Patterns
- **Dispatch Engine:** `CPUCore::step()` captures start cycles, fetches opcode, dispatches via `static const std::array<Handler, 256>`, and fills remaining cycles using start/end delta tracking to strictly enforce `kOpcodeTable` timing.
- **Handler Mapping:** ~55 specialized opcodes use dedicated inline handlers; all remaining unregistered valid opcodes fall through to `dispatch_generic()`, which resolves the effective address by addressing mode before executing instruction logic.
- **Unmapped / Illegal Opcodes:** Unregistered/illegal bytes fall back to NOP timing (+2 cycles); future phases will support explicit faulting or illegal opcode profiling.
- **Memory Layout:** Von Neumann model over a flat 64K address space (`SystemBus`). Zero Page ($00–$FF) wraps at byte boundaries, and Stack Page ($0100–$01FF) uses an 8-bit descending pointer (`SP`).
- **Interrupt Model:** Interrupts are polled AFTER instruction execution in `step()`. NMI and IRQ signals are sampled via `bus.consume_nmi()` and `bus.get_irq_line()` respectively, then dispatched to `signal_nmi()` or `signal_irq()`.

## Interrupt Handling Implementation
- **`signal_nmi()`:** Vectors to $FFFA-$FFFB, pushes status with Break=0, does NOT set I-flag (current behavior — needs fix)
- **`signal_irq()`:** Only fires if I-flag is clear, vectors to $FFFE-$FFFF, pushes status with Break=0, sets I-flag
- **`h_brk()`:** Software interrupt, vectors to $FFFE-$FFFF, pushes PC+2, pushes status with Break=1, sets I-flag
- **`trigger_interrupt()`:** Unified core routine handling stack pushes (PC high, PC low, status), vector fetch, and cycle accounting (target 7 total cycles)

## Active Edge Cases & Known Gaps
- **Page-Cross Cycle Penalties:** Absolute indexed (`ABX`/`ABY`) and Indirect Indexed (`(zp),Y`) modes must dynamically add a +1 cycle penalty when crossing a page boundary during read operations.
- **Branch Timing Delta:** Relative branching requires dynamic cycle addition (2 cycles base, +1 if branch taken, +1 additional if target crosses page boundary).
- **Zero Page Pointer Wrapping:** Indexed Indirect `(zp,X)` pointer arithmetic must wrap within Zero Page ($00–$FF) when calculating address vectors.
- **Memory Read-Modify-Write (RMW):** Memory-direct RMW instructions (`INC`, `DEC`, `ASL`, `LSR`, `ROL`, `ROR`) operating on indexed modes require exact write/RMW cycle modeling (+1 always on indexed write).
- **BCD Arithmetic:** Decimal mode (`D` flag) behavior for `ADC` and `SBC` requires dedicated BCD flag and result validation tests.

## Immediate Focus: Interrupt Bug Fixes
**4 failing tests** related to interrupt handling:

### Root Cause Analysis
1. **`MOS6502CoreTest.IRQ_ExecutesWhenEnabled_PushesStateAndVectors`** — Cycle count mismatch (expected 7, actual 8+). Interrupt is polled after instruction execution, so the NOP executes first (2 cycles) then IRQ (7 cycles) = 9 total, but test expects 7.
2. **`MOS6502CoreTest.NMI_IgnoresInterruptDisableFlag_VectorsToFFFA`** — Same cycle issue + NMI does not set I-flag (current `signal_nmi()` passes `set_i_flag=false`).
3. **`CPUFullInterruptRequirementsTest.SimultaneousNMIAndIRQFavorsNMI`** — NMI doesn't set I-flag, so after NMI service, IRQ fires on next step and overrides the NMI vector.
4. **`CPUFullInterruptRequirementsTest.NMIFiresInsideBRKHandler`** — Cascading from NMI I-flag issue.

### Required Fix
- Move interrupt polling to the **START** of `step()`, before opcode fetch — this satisfies the 7-cycle expectation (interrupt replaces the instruction)
- Fix `signal_nmi()` to set I-flag (`set_i_flag = true`) — correct 6502 hardware behavior
- Adjust test expectations for the two CPUFullInterruptRequirementsTest cases that expect interrupt service in a subsequent step

## Future Roadmap
- **Standalone binary loading and execution runner** in `src/`
- **Debugging infrastructure** (tracepoints and watchpoints)
- **Custom 6502 assembler/compiler** targeting BASIC, FORTRAN, and C++ dialects — start in parallel
- **Multi-architecture support** feasibility and infrastructure
- **Instruction pipelining and cache emulation models**
- **Sparse memory mapping** via hierarchical radix trees
- **Avionics/marine protocol simulation** (MIL-STD-1553, ARINC 429, CAN, Ethernet, RS422, USB)