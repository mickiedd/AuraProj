#!/bin/zsh

set -euo pipefail

readonly SCRIPT_DIR="${0:A:h}"
source "${SCRIPT_DIR}/Scripts/macos/unreal-common.sh"

readonly ENGINE_ASSOC="$(get_engine_association)"
readonly ENGINE_ROOT="$(resolve_engine_root "${ENGINE_ASSOC}")"
readonly UAT_SCRIPT="$(unreal_uat_script "${ENGINE_ROOT}")"
readonly ARCHIVE_DIRECTORY="${PROJECT_ROOT}/Build/MacNoEditor"

echo "Project: ${UPROJECT_PATH}"
echo "Engine:  ${ENGINE_ROOT}"
echo "UAT:     ${UAT_SCRIPT}"
echo "Output:  ${ARCHIVE_DIRECTORY}"
echo "Target:  Aura (Mac Shipping)"

"${UAT_SCRIPT}" BuildCookRun -project="${UPROJECT_PATH}" -noP4 -platform=Mac -clientconfig=Shipping -build -cook -stage -package -archive -archivedirectory="${ARCHIVE_DIRECTORY}" -pak -iostore -prereqs -target=Aura

echo "Build completed."