# AuraProj — long-term project notes

Index of durable rules. **UE/Git/build trap detail lives in `REFERENCE.md`** (same
directory) — read it when a trap bites. Job detail lives in the Change-Archive records
and in `.workbuddy-ai/memory/YYYY-MM-DD.md`.

## Landmark library

`Content/Assets/Environment/GuangzhouLandmarks/`. **Live list: `LANDMARKS` in
`Scripts/CreateGuangzhouLandmarkShowcase.py`; per-gate geometry in
`Saved/RawModelImport/guangzhou-landmark-showcase.json`.** Nine gates: Zhengnanmen
HighFidelity, Xiaobeimen AAA V3, Guidemen ReferenceRepaired, Wuxianmen V5 FullPBR,
GreatNorthGate, ZhenhaiTower, Wenmingmen, Zhengximen (2026-09-25), Zhengdongmen
(Great East Gate, 2026-09-25).

Retired 2026-09-22, whole folders deleted: `V3/Xiaobeimen_Production_V3` (176 assets),
`V5/Wuxianmen_4K_Core` (56), `BP_Guidemen_V5_4K`. Surviving Guidemen is
`V5/Guidemen_4K/BP_Guidemen_V5_4K_PreRebuild_20260918` — it carries the later
window/roof/arch/door/plaque repairs, so "PreRebuild" does **not** mean stale.
Deliberately unwrapped: `Xiaobeimen/SM_Xiaobeimen`, `SM_Xiaobeimen_GeometryFixed`.

## Placing a landmark in the ring

Never hand-place — the level is generated.

1. Add a `LANDMARKS` entry `(key, relative asset path, display name, facing_offset)`
   **and** the matching `Landmark_<key>` to `EXPECTED_LABELS` (light labels derive from
   that list, so the two move together). Rebuild, validate in a **fresh** process →
   0 errors / 0 warnings.
2. **Facing must be measured, not assumed.** Convention is front on local **-Y**, but two
   gates break it, for opposite reasons. **Wenmingmen is +Y** (`facing_offset 180.0`) — the
   model itself faces +Y — and stood back to front for a day when assumed at 0.0.
   **Zhengdongmen is also +Y** (`facing_offset 180.0`), but its *source* faces -Y: **the
   import negates Y**, so the facade arrives on +Y. Read it in the model's own frame;
   `Light_<key>` marks the plaza side. On a canal/bridge model the
   **door-is-on-the-canal-side relation is rotation-invariant**. For a package with
   **modular per-material GLBs, read the GLB's POSITION accessor bounds** — the plaque/door
   group's Y sign gives the facade (Zhengximen: plaque Y -3.055..-2.915 m, door
   Y -0.614..-0.387 m → -Y, offset 0.0).
   **But a size test cannot see a mirror**: `(x, -y, z)` and `(x, y, z)` have identical
   extents, so an import can match its GLB bounds to 2% while standing back to front. Only a
   **signed, asymmetric feature** distinguishes them — for Zhengdongmen the plaque
   (source Y -598.2..-589 → imported +589..+598.2), the door studs (+204..+211.5 →
   -211.5..-204) and the stone base centre (-4.5 → +4.5). Template:
   `Scripts/ProbeWenmingmenPlacement.py`; per-landmark assertion:
   `ValidateZhengdongmenLandmark.py`.
3. Changing **only** a facing offset moves nothing else. **Adding** a landmark re-spaces
   the whole ring — derived radius, arithmetic not a regression.
4. `FACADE_LOCAL_AXIS` in `ValidateGuangzhouLandmarkShowcase.py` asserts facing. Keep it
   current: it checked everything *except* orientation for a day.

## Environment

- **Boot volume full breaks every shell command**, not just writes: the sandbox cannot
  create its SBPL temp file under `/var/folders/.../T` → `os error 28` before the command
  runs. `Edit`/`Write` fail on the *boot* volume but work on `/Volumes/M2`.
  **Workaround: `dangerouslyDisableSandbox` on the Bash call.** `/tmp` is on the full
  volume — stage large packages under `/Volumes/M2`.
- **Use an isolated Python commandlet, not the live editor:**
  `"/Volumes/M2/Engine/UE_5.5/Engine/Binaries/Mac/UnrealEditor-Cmd" <abs>/Aura.uproject
  -run=pythonscript -script=<abs script> -unattended -nopause -nosplash -nullrhi -stdout`.
  **`print()`/`unreal.log` do not reliably reach its stdout — always write a JSON report
  and read that.** The exit code can be non-zero from the UnrealTraceServer fork even on
  success; judge by the report and the log's `Python script executed successfully` vs
  `executed with errors`.
- **UE Python remote execution does not work here.** Multicast discovery to
  `239.0.0.1:6766` never answers although the editor binds it and
  `PythonRemoteExecution=True` is set; **`adb` also holds `127.0.0.1:6766`**.
  `Scripts/remote_run.py` reports "no remote editor node discovered" sandboxed or not.
  A commandlet has **no RHI**, so rendering captures need the editor open.
- A closed editor leaves a **stale** in-memory level copy after a rebuild: say so, and
  tell the user to reload rather than save over it.

## Conventions

Landmark Blueprint = `PackedLevelActor`, one `HierarchicalInstancedStaticMeshComponent`
per source mesh, source transforms as instance transforms, root a
`LevelInstanceComponent` (`CreateGreatSouthGateActorAsset.py`,
`WrapLandmarkMeshBlueprints.py`; Zhengximen = 7 HISMs, one per material group).
`BlueprintEditorLibrary.compile_blueprint` returns `None` — verify by spawning.
**A Blueprint is saved before its components are added**, so a crashed run leaves a
wrapper that looks real and contains nothing: the reuse path must verify the component
count and rebuild, not trust the asset's existence.

## Levels

- `Scifi_desert_city/Level/L_showcase_level` — 1786 actors; older parallel park.
- `Assets/Environment/GuangzhouLandmarks/L_GuangzhouLandmarkShowcase` — the ring
  (`CreateGuangzhouLandmarkShowcase.py` builds, `ValidateGuangzhouLandmarkShowcase.py`
  checks). 8 landmarks, 8000 m ground. Each owns one attached shadow-casting spot light
  (`Light_<key>`, tag `GuangzhouLandmarkLight`, all parameters ratios of its own
  geometry). **Radius derived**: `sum(2 x radius_xy + 3000) / 2pi`.

## Retiring a landmark

1. Drop from `LANDMARKS` and `EXPECTED_LABELS`.
2. Remove its actor from **every** referencing level — `find_package_referencers_for_asset`
   first; `L_showcase_level` was a second, easy-to-miss owner.
3. Rebuild then validate → 0 errors / 0 warnings.
4. Delete via a `Scripts/Retire*.py` template: refuses while a referencer remains,
   verifies the asset gone rather than trusting `delete_asset`, tests referencers
   *outside* the folder being deleted (a folder's preview map and Blueprint reference
   each other). **Per-asset `delete_asset`, leaf-first, batched — never
   `delete_directory`** (REFERENCE trap 12). Resumable: report what is left.
5. Change-Archive record.

**Open debt — scripts still naming retired paths:** `AuditGuangzhouLandmarks{,V2}.py`,
`ExportLandmarkMeshes.py`, `FixGuangzhouLandmarksNanite.py`,
`InspectLandmarkBlueprintDetail.py`, `SnapshotV5ReferenceTuning.py`,
`ValidateGuangzhouLandmarksCrossBuilding.py`, `ValidateV5ReferenceTuning.py`,
`ProbeShowcaseLightingReference.py`.

## Shared editor

One live editor, shared by all agents. Serialize heavy passes; a resident `codex.exe` is
normal (judge activity from artifact mtimes). Before switching levels confirm
`get_dirty_content_packages()` and `get_dirty_map_packages()` are both empty.
`UE_ENGINE_ROOT` on this Mac is **`/Volumes/M2/Engine/UE_5.5`** (scripts default to a
stale Windows path). Remote-exec output arrives as a Python list repr with escaped
`\r\n` — capture to a file and split on `{'type': 'Info', 'output': '`.
