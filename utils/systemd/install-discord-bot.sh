#!/usr/bin/env bash
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Run this script as root." >&2
  exit 1
fi

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd "${script_dir}/../.." && pwd)

install -m 0755 "${root_dir}/utils/discord_lottery_bot.py" /usr/local/bin/ninacatcoin-discord-bot
install -m 0644 "${root_dir}/utils/systemd/ninacatcoin-lottery-bot.service" /etc/systemd/system/ninacatcoin-lottery-bot.service
install -d -m 0755 /etc/ninacatcoin

if [ -n "${NINACATCOIN_DISCORD_WEBHOOK:-}" ]; then
  cat > /etc/ninacatcoin/discord-bot.env <<EOF
NINACATCOIN_DISCORD_WEBHOOK=${NINACATCOIN_DISCORD_WEBHOOK}
NINACATCOIN_RPC_URL=http://127.0.0.1:19081/json_rpc
NINACATCOIN_POLL_SECONDS=15
NINACATCOIN_EXPLORER_URL=https://explorer.ninacatcoin.com
# NINACATCOIN_POST_ALL=1
# NINACATCOIN_STATE_FILE=/var/lib/ninacatcoin/discord-lottery-bot.state.json
# NINACATCOIN_SELF_RPC=http://127.0.0.1:19081/json_rpc
# NINACATCOIN_FAILOVER_RPCS=http://node1:19081/json_rpc,http://node2:19081/json_rpc
# NINACATCOIN_FAILOVER_CHECK_SECONDS=30
EOF
  chmod 600 /etc/ninacatcoin/discord-bot.env
elif [ ! -f /etc/ninacatcoin/discord-bot.env ]; then
  install -m 0600 "${root_dir}/utils/conf/discord-bot.env" /etc/ninacatcoin/discord-bot.env
fi

hook_value="$(grep -E '^NINACATCOIN_DISCORD_WEBHOOK=' /etc/ninacatcoin/discord-bot.env | head -n1 | cut -d= -f2-)"
if [ -z "${hook_value}" ]; then
  echo "Set NINACATCOIN_DISCORD_WEBHOOK in /etc/ninacatcoin/discord-bot.env, then run:"
  echo "  systemctl enable --now ninacatcoin-lottery-bot.service"
  exit 0
fi

systemctl daemon-reload
systemctl enable --now ninacatcoin-lottery-bot.service
