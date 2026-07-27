include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Explicit target declaration
add_executable(hws_tests test_cpu_core.cpp
  test_cpu_core_interrupts.cpp
  test_klaus_functional.cpp)

  # In your tests/CMakeLists.txt, right where your test executable is defined:
target_compile_definitions(hws_tests PRIVATE 
    TEST_ROM_PATH="${CMAKE_CURRENT_SOURCE_DIR}/6502_functional_test.bin"
)

# Target-based linking: automatically inherits include/ directories from hws_core
target_link_libraries(hws_tests 
    PRIVATE 
        hws_core 
        gtest_main
)

target_compile_options(hws_tests PRIVATE -Wall -Wextra -Wpedantic)

add_custom_command(
    TARGET hws_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/6502_functional_test.bin"
            "$<TARGET_FILE_DIR:hws_tests>/6502_functional_test.bin"
)

# Let CTest track our binary location natively
add_test(NAME HwsCoreSimulatorTests COMMAND hws_tests)