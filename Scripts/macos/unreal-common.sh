#!/bin/zsh

set -euo pipefail

if [[ -z "${UNREAL_COMMON_DIR:-}" ]]; then
  readonly UNREAL_COMMON_DIR="${${(%):-%x}:A:h}"
fi

if [[ -z "${PROJECT_ROOT:-}" ]]; then
  readonly PROJECT_ROOT="${UNREAL_COMMON_DIR:h:h}"
fi

if [[ -z "${UPROJECT_PATH:-}" ]]; then
  readonly UPROJECT_PATH="${PROJECT_ROOT}/Aura.uproject"
fi

function die() {
  echo "Error: $*" >&2
  exit 1
}

function ensure_project_exists() {
  [[ -f "${UPROJECT_PATH}" ]] || die "Could not find ${UPROJECT_PATH}"
}

function trim() {
  local value="$1"
  value="${value#${value%%[![:space:]]*}}"
  value="${value%${value##*[![:space:]]}}"
  printf '%s' "${value}"
}

function get_engine_association() {
  ensure_project_exists
  local association
  association="$({ grep -m1 '"EngineAssociation"' "${UPROJECT_PATH}" || true; } | sed -E 's/.*: *"([^"]+)".*/\1/')"
  [[ -n "${association}" ]] || die "Failed to read EngineAssociation from ${UPROJECT_PATH}"
  printf '%s' "${association}"
}

function candidate_engine_roots() {
  local association="$1"
  local volume_engine_root

  if [[ -n "${UE_ENGINE_ROOT:-}" ]]; then
    printf '%s\n' "${UE_ENGINE_ROOT}"
  fi

  if [[ -n "${UE_EDITOR_APP:-}" ]]; then
    printf '%s\n' "${UE_EDITOR_APP:A:h:h:h}"
  fi

  printf '%s\n' \
    "/Users/Shared/Epic Games/UE_${association}" \
    "/Users/Shared/EpicGames/UE_${association}" \
    "/Users/Shared/UnrealEngine/UE_${association}" \
    "/Applications/UE_${association}"

  for volume_engine_root in /Volumes/*/Engine/UE_${association}(N); do
    printf '%s\n' "${volume_engine_root}"
  done
}

function resolve_engine_root() {
  local association="$1"
  local candidate
  local install_ini="${HOME}/Library/Application Support/Epic/UnrealEngine/Install.ini"

  while IFS= read -r candidate; do
    candidate="$(trim "${candidate}")"
    [[ -n "${candidate}" ]] || continue
    if [[ -x "${candidate}/Engine/Build/BatchFiles/Mac/Build.sh" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  done < <(candidate_engine_roots "${association}")

  if [[ -f "${install_ini}" ]]; then
    candidate="$({ grep -E "^${association}=" "${install_ini}" || true; } | head -n1 | cut -d'=' -f2-)"
    candidate="$(trim "${candidate}")"
    if [[ -n "${candidate}" && -x "${candidate}/Engine/Build/BatchFiles/Mac/Build.sh" ]]; then
      printf '%s' "${candidate}"
      return 0
    fi
  fi

  die "Unable to locate Unreal Engine ${association}. Set UE_ENGINE_ROOT or UE_EDITOR_APP before running this script."
}

function unreal_editor_app() {
  local engine_root="$1"
  local editor_app="${engine_root}/Engine/Binaries/Mac/UnrealEditor.app"
  [[ -d "${editor_app}" ]] || die "Could not find ${editor_app}"
  printf '%s' "${editor_app}"
}

function unreal_editor_binary() {
  local editor_app="$1"
  local editor_binary="${editor_app}/Contents/MacOS/UnrealEditor"
  [[ -x "${editor_binary}" ]] || die "Could not find ${editor_binary}"
  printf '%s' "${editor_binary}"
}

function unreal_build_script() {
  local engine_root="$1"
  local build_script="${engine_root}/Engine/Build/BatchFiles/Mac/Build.sh"
  [[ -x "${build_script}" ]] || die "Could not find ${build_script}"
  printf '%s' "${build_script}"
}

function ensure_engine_stub_link() {
  local stub_path="$1"
  local source_path="$2"

  if [[ -e "${stub_path}" ]]; then
    return 0
  fi

  [[ -f "${source_path}" ]] || die "Could not find engine binary required for stub ${source_path}"

  # A stale symlink can survive an engine move. Remove only that exact broken
  # link, while refusing to overwrite any real file at the expected path.
  if [[ -L "${stub_path}" ]]; then
    rm "${stub_path}"
  fi

  ln -s "${source_path}" "${stub_path}"
  echo "Restored engine stub: ${stub_path}"
}

function ensure_editor_engine_stubs() {
  local engine_root="$1"
  local stub_dir="${engine_root}/Engine/Intermediate/Mac/Stubs/arm64/Development"
  local engine_bin_dir="${engine_root}/Engine/Binaries/Mac"
  local online_bin_dir="${engine_root}/Engine/Plugins/Online/OnlineSubsystem/Binaries/Mac"

  mkdir -p "${stub_dir}"

  # Installed UE builds provide the real dylibs, while UBT links project
  # modules through these architecture/configuration-specific stub paths.
  # Keep the links local to the engine's generated Intermediate directory.
  ensure_engine_stub_link "${stub_dir}/UnrealEditor-Projects.dylib" "${engine_bin_dir}/UnrealEditor-Projects.dylib"
  ensure_engine_stub_link "${stub_dir}/UnrealEditor-CoreOnline.dylib" "${engine_bin_dir}/UnrealEditor-CoreOnline.dylib"
  ensure_engine_stub_link "${stub_dir}/UnrealEditor-NetCore.dylib" "${engine_bin_dir}/UnrealEditor-NetCore.dylib"
  ensure_engine_stub_link "${stub_dir}/UnrealEditor-OnlineSubsystem.dylib" "${online_bin_dir}/UnrealEditor-OnlineSubsystem.dylib"
}

function unreal_uat_script() {
  local engine_root="$1"
  local uat_script="${engine_root}/Engine/Build/BatchFiles/RunUAT.sh"
  [[ -x "${uat_script}" ]] || die "Could not find ${uat_script}"
  printf '%s' "${uat_script}"
}
