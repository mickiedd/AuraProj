# Canton provisional wall height doubled

All saved adaptive wall modules in `L_Canton_WalledCity_PROVISIONAL` now have a 1,000 cm height, double the previous 500 cm. The editor update moves each module upward by its old half-height while preserving the original XY transform, pitch, and buried terrain contact line. The generator and gate-fit configuration are updated to keep 1,000 cm on future rebuilds, and the validator now requires that height.

Fresh reload validation passed for 4,790 modules: zero positive terrain gaps, no coarse wall actors, collision enabled, and the `Wall_Foundation_Adjustment` layer intact. Disk-save validation reloaded all 9 landmarks and 4,790 adaptive walls with no dirty Canton packages remaining. The map-specific `CantonWalledCityGameMode` was reasserted in World Settings before the final cook; the scoped cook processed 1,532/1,532 packages, and the standalone player camera passed at 1280×720 with a 0.9996 nonblack fraction.

Evidence: [double-height application](../../QA/Canton_Continuation/Walled_DoubleHeight.json), [terrain-fit validation](../../QA/Canton_Continuation/Walled_Wall_Terrain_Fit_Validation.json), [disk-save validation](../../QA/Canton_Continuation/Walled_Disk_Save.json), [cook report](../../QA/Canton_Continuation/Delivery_Cook.json), and [runtime visibility](../../QA/Canton_Continuation/Walled_Runtime_Visibility_Check.json).

[Diagram](2026-09-30-canton-wall-height-double.svg)
