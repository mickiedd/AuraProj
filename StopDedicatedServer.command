#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
readonly PROJECT_PATH="${SCRIPT_DIR}/Aura.uproject"

if [[ ! -f "${PROJECT_PATH}" ]]; then
	echo "Could not find project file:"
	echo "  ${PROJECT_PATH}"
	exit 1
fi

readonly PROJECT_BASENAME="${PROJECT_PATH:t}"
typeset -a MATCHING_PIDS

while IFS= read -r PID _; do
	MATCHING_PIDS+=("${PID}")
done < <(pgrep -af "UnrealEditor.*${PROJECT_BASENAME}.*-server")

if [[ ${#MATCHING_PIDS[@]} -eq 0 ]]; then
	echo "No dedicated server processes found."
	exit 0
fi

for PID in "${MATCHING_PIDS[@]}"; do
	echo "Stopping PID ${PID}"
	kill -TERM "${PID}" 2>/dev/null || true
done

sleep 1

for PID in "${MATCHING_PIDS[@]}"; do
	if kill -0 "${PID}" 2>/dev/null; then
		echo "Force stopping PID ${PID}"
		kill -KILL "${PID}" 2>/dev/null || true
	fi
done