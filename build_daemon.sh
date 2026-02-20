#!/bin/bash
# Build script for ninacatcoin daemon
set -e

BUILD_DIR="/mnt/i/ninacatcoin/build-linux"
SRC_DIR="/mnt/i/ninacatcoin"

cd "$BUILD_DIR"

echo "=== Running cmake ==="
cmake "$SRC_DIR" 2>&1 | tail -20

echo ""
echo "=== Building daemon ==="
make -j2 daemon 2>&1 | tail -40

echo ""
echo "=== Result ==="
ls -la "$BUILD_DIR/bin/ninacatcoind"
echo "=== Done ==="
