# Native GSM re-applied

## Intent

Restore the native C++ Game Server Manager and embedded WebView2 dashboard after the previous implementation was reverted.

## Changed behavior

The standalone `AuraGSM.exe`, native launcher, CMake build, dashboard assets, tests, vendored dependencies, and implementation plan are restored from the prior reviewed snapshot. The Unreal Editor Windows menu again displays `Start Native GSM (C++)`, starts `StartNativeGSM.bat`, and supplies the current editor executable. Native editor discovery requires no `UE_EDITOR_EXE` configuration and validates the matching engine and editor variant.

## Validation

`Scripts/BuildNativeGSM.ps1` produced the Release executable. `python -B Tools/GSM/tests/integration.py` passed all 11 tests covering readiness, lifecycle, authentication, metrics, editor discovery and stale environment handling. An isolated Unreal editor translation-unit compile also passed with the installed UE5.5 toolchain. The editor menu change is scoped to the GSM action; unrelated working-tree changes were preserved.

[Behavior diagram](2026-09-10-native-gsm-reapplied.svg)
