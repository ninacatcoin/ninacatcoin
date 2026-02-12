#!/bin/bash
# Example: Run checkpoint generator with SFTP credentials from environment
# This shows how to provide SFTP credentials WITHOUT editing the script

# Set environment variables (don't edit the script, just set env vars)
export SFTP_HOST="ninacatcoin.es"
export SFTP_USER="your_username"
export SFTP_PASSWORD="your_password"
export SFTP_PATH="/checkpoints/"

# Now run the script - it will read from env vars
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bash "$SCRIPT_DIR/run_checkpoint_daemon_testnet.sh"
