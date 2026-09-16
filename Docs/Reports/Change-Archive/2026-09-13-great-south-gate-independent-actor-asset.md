# Great South Gate V2 independent Actor Asset

## Intent

Save the repaired Great South Gate V2 as a portable, independent Unreal Actor Asset so it can be placed in other levels without depending on the preview map's wrapper hierarchy or preview lighting.

## Changed behavior

- Added `BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset` under `/Game/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity`.
- Created the asset as a `PackedLevelActor` Blueprint with 114 hierarchical-instanced mesh components and 12,387 mesh instances.
- Preserved all 12,195 imported model actors plus the 192 repaired solid infill panels, including their world-space transforms and material assignments.
- Excluded the preview wrapper root, `DirectionalLight`, and `SkyLight`; the saved asset is self-contained geometry rather than a preview-level lighting setup.

## Validation

- `C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe C:\Git\AuraProj\_TempAssetHost\AssetTools.uproject -run=pythonscript -Script=C:\Git\AuraProj\Scripts\CreateGreatSouthGateActorAsset.py -unattended -nop4 -nullrhi -nosound -nosplash -NoCompile` — asset created; 114 HISM components and 12,387 instances.
- Same UE 5.5 content-only host with `Scripts/ValidateGreatSouthGateActorAsset.py` — PASS after reopening the serialized Blueprint; expected HISM and instance counts match and empty mesh components = 0.
- `python -m py_compile Scripts/CreateGreatSouthGateActorAsset.py Scripts/ValidateGreatSouthGateActorAsset.py` — PASS.
- Post-job visual check: the saved SVG below was inspected for readable source → asset → validation flow, asset counts, exclusions, and placement behavior.

## Evidence

- Actor Asset: `Content/Assets/Environment/GuangzhouLandmarks/GreatSouthGate_Zhengnanmen_HighFidelity/BP_GreatSouthGate_Zhengnanmen_V2_ActorAsset.uasset`
- Creation report: `Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-actor-asset.json`
- Reload validation: `Saved/RawModelImport/GreatSouthGate_Zhengnanmen_V2-actor-asset-validation.json`
- Asset SHA-256: `7034D2F88560B7AFB984568330839D17AAEB2B14A0469D6D5CFFD8AC9B910EDD`
- Illustration: [2026-09-13-great-south-gate-independent-actor-asset.svg](2026-09-13-great-south-gate-independent-actor-asset.svg)

## Limitation

The project-native commandlet path was not used for the final serialization because the source project editor modules were unavailable after an unrelated `BehaviorURuntime` link failure. The asset creation and reload validation were completed with the installed UE 5.5.4 content-only editor host; the reusable creator/validator scripts remain in `Scripts/`.
