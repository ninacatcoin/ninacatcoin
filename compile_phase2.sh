#!/bin/bash
# Compile Phase 2 components only (for testing)
# Usage: ./compile_phase2.sh

set -e  # Exit on error

cd "$(dirname "$0")"

echo "========================================"
echo "Compiling Phase 2 Components"
echo "========================================"

# Compiler settings
CXX="g++"
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"
INCLUDES="-I./src -I./tools -I./external/rapidjson/include -I./external/easylogging"

echo ""
echo "1. Compiling security_query_tool.cpp..."
$CXX $CXXFLAGS $INCLUDES -c tools/security_query_tool.cpp -o security_query_tool.o
if [ $? -eq 0 ]; then echo "   ✅ OK"; else echo "   ❌ FAILED"; exit 1; fi

echo ""
echo "2. Compiling reputation_manager.cpp..."
$CXX $CXXFLAGS $INCLUDES -c tools/reputation_manager.cpp -o reputation_manager.o
if [ $? -eq 0 ]; then echo "   ✅ OK"; else echo "   ❌ FAILED"; exit 1; fi

echo ""
echo "3. Compiling checkpoints.cpp with Phase 2 integration..."
$CXX $CXXFLAGS $INCLUDES -c src/checkpoints/checkpoints.cpp -o checkpoints.o
if [ $? -eq 0 ]; then echo "   ✅ OK"; else echo "   ❌ FAILED"; exit 1; fi

echo ""
echo "4. Compiling integration tests..."
$CXX $CXXFLAGS $INCLUDES -c tests/unit_tests/checkpoints_phase2.cpp -o checkpoints_phase2.o
if [ $? -eq 0 ]; then echo "   ✅ OK"; else echo "   ❌ FAILED"; exit 1; fi

echo ""
echo "========================================"
echo "✅ All components compiled successfully!"
echo "========================================"
echo ""
echo "Generated object files:"
ls -la *.o 2>/dev/null | awk '{print "   " $9 " (" $5 " bytes)"}'

echo ""
echo "Ready for linking and testing."
