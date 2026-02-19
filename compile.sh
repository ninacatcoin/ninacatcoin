#!/bin/bash
cd /mnt/i/ninacatcoin/build-linux
echo "Starting compilation..."
make daemon -j4
echo "Compilation status: $?"
if [ -f bin/daemon ]; then
    echo "✓ Daemon compiled successfully"
    ls -lh bin/daemon
else
    echo "✗ Daemon compilation failed"
fi
