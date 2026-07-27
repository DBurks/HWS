cmake_minimum_required(VERSION 3.24) # Modern CMake policy baseline
project(Hardware_Simulator_Suite VERSION 1.0.0 LANGUAGES CXX)

# Support modern timestamp rules for fetched modules
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# Trace logging toggle (Defaults to ON for development/debug flexibility)
option(HWS_ENABLE_TRACE "Enable zero-overhead CPU instruction tracing" ON)

# 1. Create an Interface Library for headers to formalize dependency propagation
add_library(hws_core INTERFACE)
target_include_directories(hws_core INTERFACE include)

# Enforce C++20 transitively down to everything linking against the core engine
target_compile_features(hws_core INTERFACE cxx_std_20)

# Pass the trace switch down via compile definitions
if(HWS_ENABLE_TRACE)
  target_compile_definitions(hws_core INTERFACE HWS_ENABLE_TRACE=1)
else()
  target_compile_definitions(hws_core INTERFACE HWS_ENABLE_TRACE=0)
endif()

# 2. Configure Main Executable
add_executable(hws_sim src/main.cpp)
target_link_libraries(hws_sim PRIVATE hws_core)

# Guard compilation flags down to individual targets
target_compile_options(hws_sim PRIVATE -Wall -Wextra -Wpedantic)

# 3. Handle testing scope isolation cleanly
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  enable_testing()
  add_subdirectory(tests)
endif()