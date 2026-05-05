#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

readonly ENGINE_ASSOC="$(get_engine_association)"
readonly ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")"
readonly BUILD_SCRIPT="$(unreal_build_script "${ENGINE_ROOT}")"

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "Target:  AuraEditor (Mac Development)"

"${BUILD_SCRIPT}" AuraEditor Mac Development "${UPROJECT_PATH}"

echo "Build completed."