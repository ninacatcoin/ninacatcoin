@echo off
REM Checkpoint Generator Daemon for TESTNET - Windows Batch Script
REM This script generates ninacatcoin checkpoints every 2 hours
REM
REM The script will:
REM   - Generate checkpoints automatically every 2 hours
REM   - Save to checkpoints_testnet_updated.json locally
REM   - Optionally upload to SFTP (see below)
REM
REM Optional SFTP Upload:
REM   If you want automatic SFTP upload, uncomment the SFTP lines below
REM   and set your credentials

REM ============ CONFIGURATION ============
SET NETWORK=TESTNET
SET DAEMON_RPC=http://localhost:29081
SET DAEMON_INTERVAL=7200
SET CONFIRMATIONS=50

REM Optional SFTP (comment out if not needed)
REM SET SFTP_HOST=ninacatcoin.es
REM SET SFTP_USER=your_username
REM SET SFTP_PASSWORD=your_password
REM SET SFTP_PATH=/checkpoints/
REM ========================================

echo.
echo ========================================
echo Checkpoint Generator Daemon - TESTNET
echo ========================================
echo.
echo Configuration:
echo   Network:     %NETWORK%
echo   Daemon RPC:  %DAEMON_RPC%
echo   Interval:    %DAEMON_INTERVAL%s (2 hours)
echo   SFTP:        %if defined SFTP_HOST%(Enabled) else (Disabled - local only)%
echo.

echo Starting checkpoint generator daemon...
echo Press Ctrl+C to stop
echo.

cd /d "%~dp0"

python "%~dp0contrib\checkpoint_generator.py" ^
    --daemon-mode ^
    --network %NETWORK% ^
    --daemon-rpc %DAEMON_RPC% ^
    --daemon-interval %DAEMON_INTERVAL% ^
    --confirmations %CONFIRMATIONS%

if errorlevel 1 (
    echo.
    echo ERROR: Checkpoint generator failed!
    echo Check that:
    echo   1. Python 3 is installed
    echo   2. TESTNET daemon is running on port 29081
    echo.
    pause
    exit /b 1
)
