# Copyright (c) 2026, The ninacatcoin Project
#
# Calculates SHA-256 hash of ALL source files that compose the daemon binary.
# This provides full binary integrity verification, not just AI files.
#
# At runtime, the daemon recalculates this hash from source files on disk.
# If they differ → source was tampered → auto-download from GitHub + rebuild.

set(FULL_HASH_HEADER "${CMAKE_BINARY_DIR}/generated_include/full_source_hash.h")
set(FULL_FILELIST_PATH "${CMAKE_BINARY_DIR}/full_source_filelist.txt")

# Recursively glob ALL source files in src/
file(GLOB_RECURSE ALL_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Also include key CMakeLists and config files that affect the binary
list(APPEND ALL_SOURCE_FILES
    "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_SOURCE_DIR}/src/CMakeLists.txt"
)

# Exclude patterns: generated files, python ML service, pycache
set(FILTERED_FILES "")
foreach(F ${ALL_SOURCE_FILES})
    string(FIND "${F}" "__pycache__" PYCACHE_POS)
    string(FIND "${F}" "nina_ml" NINAML_POS)
    string(FIND "${F}" "generated_include" GENERATED_POS)
    string(FIND "${F}" "build-linux" BUILD_POS)
    if(PYCACHE_POS EQUAL -1 AND NINAML_POS EQUAL -1 AND GENERATED_POS EQUAL -1 AND BUILD_POS EQUAL -1)
        list(APPEND FILTERED_FILES "${F}")
    endif()
endforeach()

# Sort for deterministic hashing
list(SORT FILTERED_FILES)

# Write file list (one per line)
string(REPLACE ";" "\n" FILELIST_CONTENT "${FILTERED_FILES}")
file(WRITE "${FULL_FILELIST_PATH}" "${FILELIST_CONTENT}")

# Custom command: generate full_source_hash.h
add_custom_command(
    OUTPUT "${FULL_HASH_HEADER}"
    COMMAND ${CMAKE_COMMAND}
        -DAI_FILELIST=${FULL_FILELIST_PATH}
        -DOUTPUT_FILE=${FULL_HASH_HEADER}
        -DHASH_TYPE=FULL
        -P ${CMAKE_SOURCE_DIR}/cmake/GenerateFullHash.cmake
    DEPENDS ${FILTERED_FILES}
    COMMENT "[Full Integrity] Calculating SHA-256 of ALL ${FILTERED_FILES_COUNT} source files..."
)

add_custom_target(full_source_hash ALL
    DEPENDS "${FULL_HASH_HEADER}"
)

list(LENGTH FILTERED_FILES FILTERED_FILES_COUNT)
message(STATUS "[Full Integrity] Enabled: ${FILTERED_FILES_COUNT} source files → ${FULL_HASH_HEADER}")
