#!/bin/bash
# Run checkpoint generator daemon for TESTNET
# Generates checkpoints every 2 hours automatically
#
# SFTP credentials via environment variables (if needed):
#   export SFTP_HOST=ninacatcoin.es
#   export SFTP_USER=myuser
#   export SFTP_PASSWORD=mypassword
#   export SFTP_PATH=/checkpoints/
# Then run this script

python3 contrib/checkpoint_generator.py \
    --daemon-mode \
    --network TESTNET \
    --daemon-rpc http://localhost:29081 \
    --interval 1000 \
    --daemon-interval 7200 \
    --confirmations 50


