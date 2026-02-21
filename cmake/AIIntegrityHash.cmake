# Copyright (c) 2026, The ninacatcoin Project
#
# Calculates SHA-256 hash of all AI source files at compile time.
# Generates a header with the canonical hash embedded as constexpr.
#
# This solves the chicken-and-egg problem:
#   - Hash is determined at BUILD TIME
#   - All binaries built from the same source have the same hash
#   - Nodes compare their compiled hash with peers via P2P
#   - If a node has a different hash → code was modified

set(AI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/ai")
set(AI_HASH_HEADER "${CMAKE_BINARY_DIR}/generated_include/ai_code_hash.h")

# List of AI source files to hash (sorted for determinism)
set(AI_INTEGRITY_FILES
    "${AI_SOURCE_DIR}/ai_checkpoint_monitor.cpp"
    "${AI_SOURCE_DIR}/ai_checkpoint_monitor.hpp"
    "${AI_SOURCE_DIR}/ai_checkpoint_validator.cpp"
    "${AI_SOURCE_DIR}/ai_checkpoint_validator.hpp"
    "${AI_SOURCE_DIR}/ai_config.hpp"
    "${AI_SOURCE_DIR}/ai_forced_remediation.cpp"
    "${AI_SOURCE_DIR}/ai_forced_remediation.hpp"
    "${AI_SOURCE_DIR}/ai_integrity_verifier.cpp"
    "${AI_SOURCE_DIR}/ai_integrity_verifier.hpp"
    "${AI_SOURCE_DIR}/ai_lwma_learning.cpp"
    "${AI_SOURCE_DIR}/ai_lwma_learning.hpp"
    "${AI_SOURCE_DIR}/ai_module.cpp"
    "${AI_SOURCE_DIR}/ai_module.hpp"
    "${AI_SOURCE_DIR}/ai_network_sandbox.cpp"
    "${AI_SOURCE_DIR}/ai_network_sandbox.hpp"
    "${AI_SOURCE_DIR}/ai_quarantine_system.cpp"
    "${AI_SOURCE_DIR}/ai_quarantine_system.hpp"
    "${AI_SOURCE_DIR}/ai_sandbox.cpp"
    "${AI_SOURCE_DIR}/ai_sandbox.hpp"
)

# Write file list to a temp file (avoids semicolon escaping issues)
set(AI_FILELIST_PATH "${CMAKE_BINARY_DIR}/ai_integrity_filelist.txt")
string(REPLACE ";" "\n" AI_FILELIST_CONTENT "${AI_INTEGRITY_FILES}")
file(WRITE "${AI_FILELIST_PATH}" "${AI_FILELIST_CONTENT}")

# Custom command: generate ai_code_hash.h by hashing all AI files
add_custom_command(
    OUTPUT "${AI_HASH_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DAI_FILELIST=${AI_FILELIST_PATH}
        -DOUTPUT_FILE=${AI_HASH_HEADER}
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateAIHash.cmake
    DEPENDS ${AI_INTEGRITY_FILES}
    COMMENT "[AI Integrity] Calculating SHA-256 of AI source files..."
)

add_custom_target(ai_integrity_hash ALL
    DEPENDS "${AI_HASH_HEADER}"
)

message(STATUS "[AI Integrity] Hash generation enabled → ${AI_HASH_HEADER}")
