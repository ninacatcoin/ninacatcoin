@echo off
REM Example: Run checkpoint generator with SFTP credentials from environment variables
REM This shows how to provide SFTP credentials WITHOUT editing the script

REM Set environment variables (don't edit the script, just set env vars)
set SFTP_HOST=ninacatcoin.es
set SFTP_USER=your_username
set SFTP_PASSWORD=your_password
set SFTP_PATH=/checkpoints/

REM Now run the script - it will read from env vars
python contrib\checkpoint_generator.py ^
    --daemon-mode ^
    --network TESTNET ^
    --daemon-rpc http://localhost:29081 ^
    --interval 1000 ^
    --daemon-interval 7200 ^
    --confirmations 50
