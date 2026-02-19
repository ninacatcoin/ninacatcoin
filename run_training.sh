#!/bin/bash
cd /mnt/i/ninacatcoin
pip3 install --break-system-packages pandas scikit-learn numpy > /tmp/pip_install.log 2>&1
echo "Dependencies installation complete"
python3 train_models.py > /tmp/train_output.log 2>&1 &
TRAIN_PID=$!
echo "Training started, PID: $TRAIN_PID"
sleep 3
tail -50 /tmp/train_output.log
