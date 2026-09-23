#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

ENGINE_ASSOC="$(get_engine_association)" || exit $?
ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")" || exit $?
BUILD_SCRIPT="$(unreal_build_script "${ENGINE_ROOT}")" || exit $?
readonly ENGINE_ASSOC ENGINE_ROOT BUILD_SCRIPT

ensure_editor_engine_stubs "${ENGINE_ROOT}"

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Target:  AuraEditor (Mac Development)"

"${BUILD_SCRIPT}" AuraEditor Mac Development "${UPROJECT_PATH}"

echo "Build completed."
