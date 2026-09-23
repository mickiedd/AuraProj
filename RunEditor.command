#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

ENGINE_ASSOC="$(get_engine_association)" || exit $?
ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")" || exit $?
EDITOR_APP="$(unreal_editor_app "${ENGINE_ROOT}")" || exit $?
readonly ENGINE_ASSOC ENGINE_ROOT EDITOR_APP

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Editor:  ${EDITOR_APP}"

open -n "${EDITOR_APP}" --args "${UPROJECT_PATH}"
