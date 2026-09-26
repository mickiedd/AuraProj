#!/bin/zsh
# Reproduce the isolated Canton prototype import/review. Requires built AuraEditor.
set -euo pipefail
source "${0:A:h}/macos/unreal-common.sh"
engine_root="$(resolve_engine_root "$(get_engine_association)")"
editor_binary="$(unreal_editor_binary "$(unreal_editor_app "${engine_root}")")"
cd "${PROJECT_ROOT}"
mode="${1:-review}"
case "${mode}" in
  import)
    python3 -c 'from pathlib import Path; Path("QA/UE_Import_Screenshots/Creation_Validation.json").unlink(missing_ok=True)'
    Saved/TerrainTools/venv/bin/python Scripts/prepare_canton_ue_import.py
    Saved/TerrainTools/venv/bin/python Scripts/test_canton_ue_import_contract.py
    "${editor_binary}" "${UPROJECT_PATH}" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities \
      "-ExecutePythonScript=${PROJECT_ROOT}/Scripts/ImportCantonProvisionalTerrain.py" \
      -unattended -nosplash -NoSound "-abslog=${PROJECT_ROOT}/Saved/Logs/CantonImport.log"
    python3 -c 'import json; assert json.load(open("QA/UE_Import_Screenshots/Creation_Validation.json"))["passed"]'
    ;;
  review)
    python3 -c 'from pathlib import Path; Path("QA/UE_Import_Screenshots/Reload_Validation.json").unlink(missing_ok=True)'
    "${editor_binary}" "${UPROJECT_PATH}" -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities \
      "-ExecutePythonScript=${PROJECT_ROOT}/Scripts/ReviewCantonProvisionalTerrain.py" \
      -unattended -nosplash -NoSound "-abslog=${PROJECT_ROOT}/Saved/Logs/CantonReview.log"
    python3 -c 'import json; assert json.load(open("QA/UE_Import_Screenshots/Reload_Validation.json"))["passed"]'
    ;;
  automation)
    python3 -c 'from pathlib import Path; Path("QA/UE_Import_Screenshots/Automation_Validation.json").unlink(missing_ok=True)'
    "${editor_binary}" "${UPROJECT_PATH}" \
      '-ExecCmds=Automation RunTests Aura.Canton.Provisional.MapConfiguration' \
      '-TestExit=Automation Test Queue Empty' "-ReportExportPath=${PROJECT_ROOT}/Saved/Reports/CantonAutomation" \
      -unattended -nosplash -NoSound "-abslog=${PROJECT_ROOT}/Saved/Logs/CantonAutomation.log"
    python3 -c 'import json; assert json.load(open("QA/UE_Import_Screenshots/Automation_Validation.json"))["passed"]'
    ;;
  *) echo 'Usage: zsh Scripts/run_canton_provisional_review.sh import|review|automation' >&2; exit 2 ;;
esac
