# Editor crashes — cause, fix, and prevention

Date: 2026-09-18.

The editor has crashed twice in this project's recent history, in two different ways. This
records what each was, what was fixed, and what is still outstanding.

[Archived diagram](2026-09-18-editor-crash-prevention.svg)

## Crash 1 — out of memory (2026-09-18 14:34) — FIXED

Caused by re-importing the Guidemen source with `bake_meshes = True`. The log names the path
outright:

```
Script Stack (1 frames): /Script/InterchangeEngine.InterchangeManager.ImportAsset
Ran out of memory allocating 1485644460 (1416.8 MiB) bytes with alignment 4.
Last error msg: The paging file is too small for this operation to complete.
PeakUsedVirtual 135348469760 (126.05 GiB)   AvailableVirtual 1.15 GiB
```

**Why.** The source is *shared-mesh instanced* — 18,823 nodes referencing 48 geometry
definitions. Baking writes every node out as its own mesh, so the import tried to materialise
the full expanded count. Preflight measured what that costs:

| source | nodes | geoms | unique triangles | baked |
| --- | --- | --- | --- | --- |
| `GreatSouthGate_Zhengnanmen_UE5_HighPoly` | 2,449 | 16 | 5,563,646 | **~2,791 GB** |
| `GreatSouthGate_Zhengnanmen_UE5_HighFidelity` | 12,196 | 112 | 179,312 | **~448 GB** |
| `Xiaobeimen_SmallNorthGate_..._500M_Instanced` | 1,552 | 6 | 576,102 | **~183 GB** |
| `Guidemen_GuideGate_UE5_100M_Instanced` | 18,823 | 48 | 38,276 | **~148 GB** |

Against a 97.8 GB commit limit, the Guidemen import could never have fitted.

### The fix

**`Scripts/ImportPreflight.py`** — audits every GLB in the project from its JSON chunk alone (no
geometry decoding, so it is fast even at 200 MB) and returns a verdict with projected gigabytes:

```
5 GLBs BAKE-FORBIDDEN   6 LARGE   the rest SAFE
```

It also reports the machine's commit headroom, because the failure mode is the commit limit
rather than physical RAM.

**A hard guard in all six bake-on-import scripts** — `ImportAndPlaceGreatWestGate`,
`ImportGreatNorthGateDabeiMen`, `ImportGreatNorthGateHighDetail`,
`ImportGreatSouthGateZhengnanmen`, `ImportGuidemen`, `ImportGuangzhouLandmarks`. The guard is
self-contained and inlined (these scripts run under Unreal's script runner, where `__file__` and
sibling imports are not dependable), sits immediately before each `bake_meshes = True`, and
refuses above a 24 GB budget with the triangles, the gigabytes and the fix in the message.

**Verified against the real sources**, not just compiled:

| case | result |
| --- | --- |
| Guidemen, `bake=True` | **REFUSED** — the crash case |
| Guidemen, `bake=False` | allowed, 38,276 triangles, 0.01 GB |
| Wuxianmen winding variant, `bake=True` | allowed, 53,736,384 triangles, 11.01 GB |
| `SM_Guidemen_Plaque` (tiny) | allowed, 48 triangles |

All six patched scripts compile.

## Crash 2 — Nanite blend-mode assertion (2026-09-15 02:37) — SCRIPTED, NOT YET RUN

```
Assertion failed: Nanite::IsSupportedBlendMode(ShadingMaterial)
NaniteShading.cpp Line 637   (3 fatal hits)
```

Nanite cannot render a material whose blend mode or shading model it does not support, and the
engine **asserts rather than degrading**. Nanite accepts Opaque and Masked, with Default Lit or
Unlit; Translucent and Additive assert.

**This matters right now**: every material built for these buildings sets
`used_with_nanite = True`. A single non-opaque material on a Nanite mesh is a live crash.

**`Scripts/AuditNaniteMaterialCompatibility.py`** scans every landmark material and flags any
that has Nanite enabled with an unsupported blend or shading mode. It is written and compiles,
but **the editor is closed, so it has not been run** — this class remains unfixed until it is.

## Crash 3 — a handled ensure, not a crash (2026-09-16)

```
Ensure condition failed: ParentNode   SimpleConstructionScript.cpp Line 960
Script Stack: /Script/SubobjectDataInterface.SubobjectDataSubsystem.DeleteSubobject
```

A *handled* ensure — logged as an error, but the editor survives. The crash reader now separates
these from fatal errors so a benign ensure no longer masquerades as a crash.

**`Scripts/ReadEditorCrashLog.py`** scans every log, classifies the cause, and prints the
surrounding context plus the likely fix — one command instead of grepping. It found both fatal
classes above, including the 09-15 one that had gone unnoticed.

## Machine headroom — the underlying limit

| | |
| --- | --- |
| physical RAM | 63.77 GB (46.5 GB available) |
| page file | 34.00 GB |
| commit limit | 97.77 GB (72.5 GB available) |
| **C: free space** | **59 GB of 1.9 TB — 97% full** |

The page file cannot grow into 59 GB of free space on a nearly full volume, which is exactly why
the failure message named the paging file rather than RAM. A 148 GB bake was never going to fit.
**Freeing space on C: is a system-level change and is left to you.**

## Before the next import

1. `python Scripts/ImportPreflight.py` — read the verdict table.
2. If it says BAKE-FORBIDDEN, keep `bake_meshes = False`; the guards now enforce this.
3. Start from a freshly opened editor with nothing else heavy running.
4. Free space on C: so the page file can grow.
5. After any crash, `python Scripts/ReadEditorCrashLog.py`.

## Status

| | |
| --- | --- |
| Out-of-memory class | **fixed and verified** |
| Nanite blend-mode class | **scripted, not yet run** — editor closed |
| Handled ensures | reclassified as non-fatal |
| Disk headroom | reported; needs your decision |
| GPU-path crashes, plugin crashes | **not addressed** — only the classes present in the logs |
