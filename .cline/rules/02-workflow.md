# Development Workflow Constraints (30-Minute Sprints)

- **Granular Modifications:** Never rewrite entire classes at once. Focus on one instruction, one addressing mode, or one isolated interface modification per turn.
- **Test-Driven Baseline:** Every new component, register modification, or opcode added must be immediately accompanied by a corresponding GoogleTest assertion block in `tests/test_cpu_core.cpp`.
- **The Anchor Rule:** At the end of a session, if work is incomplete, always provide a deliberate compilation failure or a failing placeholder test accompanied by a precise code comment tracking exactly where the mental state left off.