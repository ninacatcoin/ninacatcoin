#!/bin/bash
# Test if gcc/assembler works, then run cmake
echo "=== Testing GCC ==="
cat > /tmp/test_gcc.c << 'EOF'
int main(void) { return 0; }
EOF

gcc /tmp/test_gcc.c -o /tmp/test_gcc 2>&1
echo "GCC exit code: $?"

if [ "$?" -ne 0 ]; then
    echo "GCC broken, trying to restart WSL assembler workaround..."
fi

echo "=== Running cmake ==="
cd /mnt/i/ninacatcoin/build-linux
cmake -DBUILD_TESTS=OFF -DBUILD_DEBUG_UTILITIES=OFF .. 2>&1 | tail -20
echo "CMAKE exit: $?"

if [ "$?" -eq 0 ]; then
    echo "=== Building daemon ==="
    make -j2 daemon 2>&1 | tail -30
    echo "MAKE exit: $?"
fi
