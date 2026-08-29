#!/bin/zsh
# Aura Game Server Manager - macOS launcher
# Usage: ./StartGameServer.command [--public-host <ip>] [--port <port>] [--server-exe <path>] [--persistence-provider <name>]
# Local default: GSM loopback at 127.0.0.1; ServerConnection.json keeps the advertised serverAddress separate.
# LAN/public: set AURA_GSM_HOST, AURA_GSM_ADDRESS (client/server manager endpoint), AURA_PUBLIC_HOST,
#             AURA_GSM_AUTH_TOKEN, AURA_GSM_SERVER_AUTH_TOKEN (optional shared value),
#             and AURA_GSM_ALLOWED_CLIENTS to one or more client IPs/CIDRs before starting.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/Scripts/GameServerManager.py"

# Determine public host (defaults to 127.0.0.1; override via env for LAN/internet)
PUBLIC_HOST="${AURA_PUBLIC_HOST:-127.0.0.1}"

# Determine listen port (defaults to 9000)
GSM_PORT="${AURA_GSM_PORT:-9000}"

# Bind the control plane to loopback by default; set AURA_GSM_HOST only with an explicit LAN/public allowlist and auth policy.
GSM_HOST="${AURA_GSM_HOST:-127.0.0.1}"

# Development identity provider (defaults to NULL for the local WebUI/client harness)
PERSISTENCE_PROVIDER="${AURA_PERSISTENCE_PROVIDER:-NULL}"

# Server executable override
GSM_ARGS=(
    python3
    "$PYTHON_SCRIPT"
    --host "$GSM_HOST"
    --port "$GSM_PORT"
    --public-host "$PUBLIC_HOST"
    --persistence-provider "$PERSISTENCE_PROVIDER"
)
if [ -n "$AURA_SERVER_EXE" ]; then
    GSM_ARGS+=(--server-exe "$AURA_SERVER_EXE")
fi
GSM_ARGS+=("$@")

echo "[AuraGSM] Starting Game Server Manager on $GSM_HOST:$GSM_PORT (public-host=$PUBLIC_HOST, persistence-provider=$PERSISTENCE_PROVIDER)..."
"${GSM_ARGS[@]}"
