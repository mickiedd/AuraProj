#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

readonly ENGINE_ASSOC="$(get_engine_association)"
readonly ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")"
readonly EDITOR_APP="$(unreal_editor_app "${ENGINE_ROOT}")"
readonly EDITOR_BINARY="$(unreal_editor_binary "${EDITOR_APP}")"
readonly MAP="/Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon"

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Editor:  ${EDITOR_BINARY}"
echo "Map:     ${MAP}"
echo "Mode:    Dedicated Server"

"${EDITOR_BINARY}" "${UPROJECT_PATH}" "${MAP}" -game -server -log -unattended -NoLiveCoding