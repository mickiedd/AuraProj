#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

readonly ENGINE_ASSOC="$(get_engine_association)"
readonly ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")"
readonly EDITOR_APP="$(unreal_editor_app "${ENGINE_ROOT}")"
readonly EDITOR_BINARY="$(unreal_editor_binary "${EDITOR_APP}")"
readonly DEFAULT_MAP="/Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || "${1:-}" == "/?" ]]; then
	echo "Usage: $(basename "$0") [MapPath] [AdditionalArgs...]"
	echo
	echo "Examples:"
	echo "  $(basename "$0")"
	echo "  $(basename "$0") /Game/Maps/StartupMap -port=7777"
	echo "  $(basename "$0") /Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon -port=7778 -QueryPort=27016"
	echo
	echo "When no MapPath is provided, the script uses:"
	echo "  ${DEFAULT_MAP}"
	exit 0
fi

MAP="${1:-${DEFAULT_MAP}}"
if [[ $# -gt 0 ]]; then
	shift
fi

EXTRA_ARGS=("$@")

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Editor:  ${EDITOR_BINARY}"
echo "Map:     ${MAP}"
if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
	echo "Args:    ${EXTRA_ARGS[*]}"
fi
echo "Mode:    Dedicated Server"

"${EDITOR_BINARY}" "${UPROJECT_PATH}" "${MAP}" "${EXTRA_ARGS[@]}"