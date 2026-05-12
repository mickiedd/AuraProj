#!/bin/zsh
# Aura Game Server Manager - macOS launcher
# Usage: ./StartGameServer.command [--public-host <ip>] [--port <port>] [--server-exe <path>]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/Scripts/GameServerManager.py"

# Determine public host (defaults to 127.0.0.1; override via env for LAN/internet)
PUBLIC_HOST="${AURA_PUBLIC_HOST:-127.0.0.1}"

# Determine listen port (defaults to 9000)
GSM_PORT="${AURA_GSM_PORT:-9000}"

# Server executable override
SERVER_EXE_ARG=""
if [ -n "$AURA_SERVER_EXE" ]; then
    SERVER_EXE_ARG="--server-exe \"$AURA_SERVER_EXE\""
fi

echo "[AuraGSM] Starting Game Server Manager on port $GSM_PORT (public-host=$PUBLIC_HOST)..."
eval python3 "$PYTHON_SCRIPT" --host 0.0.0.0 --port "$GSM_PORT" --public-host "$PUBLIC_HOST" $SERVER_EXE_ARG "$@"
