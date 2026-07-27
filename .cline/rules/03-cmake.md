# Modern CMake Standards

- Never use global directives (`include_directories`, `add_definitions`). Everything must be target-based (`target_link_libraries`, `target_compile_options`).
- Maintain compatibility with `CMakePresets.json` using the Ninja generator pipeline.