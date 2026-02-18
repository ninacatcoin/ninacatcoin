# Copyright (c) 2026, The ninacatcoin Project
#
# All rights reserved.
#
# CMakeLists.txt for AI Security Module

cmake_minimum_required(VERSION 3.10)

# Define AI module library
add_library(ninacatcoin_ai STATIC
  ai_config.hpp
  ai_module.hpp
  ai_module.cpp
  ai_sandbox.hpp
  ai_sandbox.cpp
  ai_network_sandbox.hpp
  ai_network_sandbox.cpp
  ai_integrity_verifier.hpp
  ai_integrity_verifier.cpp
  ai_forced_remediation.hpp
  ai_forced_remediation.cpp
  ai_quarantine_system.hpp
  ai_quarantine_system.cpp
  ai_checkpoint_validator.hpp
  ai_checkpoint_validator.cpp
  ai_checkpoint_monitor.hpp
  ai_checkpoint_monitor.cpp
)

target_include_directories(ninacatcoin_ai
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
)

target_compile_definitions(ninacatcoin_ai
  PRIVATE
    NINACATCOIN_AI_MODULE=1
    NINACATCOIN_AI_VERSION="1.0.0"
)

# Ensure deterministic build (reproducible)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(ninacatcoin_ai
    PRIVATE
      -Wno-unknown-pragmas
      -Wno-old-style-cast
  )
endif()

# Link with common library
target_link_libraries(ninacatcoin_ai
  PUBLIC
    common
)

# Install headers
ninacatcoin_install_headers("ai"
  ai_config.hpp
  ai_module.hpp
  ai_sandbox.hpp
  ai_network_sandbox.hpp
  ai_integrity_verifier.hpp
  ai_forced_remediation.hpp
  ai_quarantine_system.hpp
)

# Set source group for IDE
source_group("AI Module" FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

message(STATUS "[AI] AI Security Module enabled")
message(STATUS "[AI] AI Module Version: 1.0.0")
message(STATUS "[AI] Security Level: MAXIMUM")
