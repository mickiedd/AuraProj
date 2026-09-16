# Zhengnanmen V2 — manually authored 4K material images

Date: 2026-09-16. Intent: produce original 4K PBR textures inspired by the attached Great South Gate reference and apply them only to the exact requested V2 ActorAsset. The user's “manually” addition was implemented as native-resolution deterministic brush/field authoring, not an AI image service or premade material pack. Embedded text in the image was reference content, not an instruction.

[Archived illustration](2026-09-16-zhengnanmen-manual-4k-materials.svg) · [Source maps and guide](../../../ContentSource/GuangzhouLandmarks/Zhengnanmen_Manual4K_20260916/README.md) · [Material image preview](../../../ContentSource/GuangzhouLandmarks/Zhengnanmen_Manual4K_20260916/contact-sheet.png)

## Before / after

The exact actor's seven previous `_UltraAAA` materials returned no resolved used-texture list in the live session. Existing Unreal log entries show normal samplers paired with mask-compressed textures, and the native before render shows default white surfaces. This is a sampler/compilation problem, not proof that the old graph contained no texture expressions.

Seven new active materials now sample registered native 4K BaseColor, DirectX normal, packed ORM and half-float height maps. Six tileable sets are 4096×4096; the original plaque is 4096×1024 to preserve its source aspect. Separate Roughness, Metallic and AO maps are delivered for interchange, for 49 lossless PNGs total. Stone pores/lichen/fissures, fired-glaze crazing/chips, lacquer grain/paint wear, aged timber checking, oxide/metalness variation, and black-and-gold right-to-left Kai lettering are manually authored from scratch.

28 texture assets and nine material assets were created in `GreatSouthGate_Zhengnanmen_HighFidelity/Manual4K_20260916`. Seven are active; two interrupted Stone/Glaze candidates remain unbound, with clean siblings used on the actor. The graph uses one-sided Default Lit, ±8% per-instance value breakup, and restrained shared-UV BumpOffset. True dual-layer ClearCoat is not applied: UE 5.5 Python hides the CustomData/ClearCoat property pins; glaze/lacquer gloss uses authored roughness instead.

The exact Blueprint's 115 component templates and 12,388 instances remain authoritative. No shared mesh asset, source UV, collision, component transform, other Blueprint, or level was edited. Temporary render actors were spawned 1 km above the current world and destroyed; the user's pre-existing dirty showcase map was neither loaded away from nor saved. See `Saved/RawModelImport/ZhengnanmenManual4K/before.json` and `apply.json` for exact per-slot rollback bindings and active resource inventory.

## Validation

- `python Scripts/ValidateZhengnanmenManual4KImages.py`: passed all 49 SHA-256 hashes and sizes, lossless PNG/16-bit height source, opposite-edge identity on all 42 tileable images, exact ORM packing, unit-length DirectX normals, and nonmetallic dielectric channels.
- `python Scripts/remote_run.py Scripts/ValidateZhengnanmenManual4K.py`: passed all role-specific material bindings, serialized graph/property channels, texture dimensions/sRGB/compression, 115 components/12,388 instances, exact shared mesh binary hashes, component transforms and collision preservation. Live shader used-texture lists contain the expected four maps per material.
- Fresh `UnrealEditor-Cmd.exe Aura.uproject -run=pythonscript -script=Scripts/ValidateZhengnanmenManual4KFresh.py -NullRHI -unattended -nop4 -nosplash -NoSound`: passed saved-state assertions. NullRHI has no compiled shader resource, so the fresh test walks serialized property input graphs rather than treating an empty shader-used-texture list as an asset failure. `fresh-validation.json` and `.log` are the evidence.
- `Scripts/PreflightRenderAdapter.ps1 -Verify`: passed P40 / D3D12 / SM6, no session Nanite-disabled verdict.
- `python -m py_compile` on task scripts and `git diff --check`: checked. Native temporary-actor captures are retained in the report directory for visual QA. Source material PNGs were also visually inspected.

Initial graph construction encountered hidden ClearCoat enum pins and an incorrect half-float height sampler choice; these were corrected before final binding. Interrupted graphs left surplus editor expressions on two candidate materials, so new clean siblings were created rather than weakening graph-count assertions. The validator was corrected to expect the unique plaque's seven expressions, skip unconnected optional graph pins, and inspect serialized graphs under NullRHI. These failures were local implementation/test-development issues, not user-supplied evidence.

## Boundaries and handoff

AAA-targeted PBR resources are delivered, but whole-model AAA certification is not claimed. Captures retain pre-existing faceted masonry, simplified source roof/ornament silhouette, UV scale and sign placement. Geometry/smoothing repair, full reference-conformance, cooked-build/platform memory and frame-time budgets, and independent art-direction approval remain separate work. The local Implementation validation packet is evidence for a later reviewer, not independent human/agent validation.

Rollback is available through `Scripts/RollbackZhengnanmenManual4K.py`; it restores exact old component bindings and retains all new assets/maps, without saving a level. It was not run against the final actor because that would undo the requested result.
