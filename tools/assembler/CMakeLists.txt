# tools/assembler/CMakeLists.txt

# Group source files
# tools/assembler/CMakeLists.txt
set(ASSEMBLER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InstructionTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Assembler.cpp
)

add_executable(hws_asm ${ASSEMBLER_SOURCES})

# Specify include directories
target_include_directories(hws_asm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Apply the same strict compiler warnings and standards you use for the simulator
target_compile_features(hws_asm PRIVATE cxx_std_23)
target_compile_options(hws_asm PRIVATE -Wall -Wextra -Wpedantic)