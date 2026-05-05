#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

readonly ENGINE_ASSOC="$(get_engine_association)"
readonly ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")"
readonly EDITOR_APP="$(unreal_editor_app "${ENGINE_ROOT}")"

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Editor:  ${EDITOR_APP}"

open -n "${EDITOR_APP}" --args "${UPROJECT_PATH}"