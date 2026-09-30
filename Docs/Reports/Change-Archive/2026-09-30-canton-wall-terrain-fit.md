# Canton provisional wall terrain fit

The wall proxy was floating over the imported modern-context landscape because each long cube used one center height and a flat bottom. The supplied terrain-fit guide was used as advisory engineering guidance; the level remains marked provisional and its historical XY/Z claims remain unaccepted.

The implementation now creates the additive `Wall_Foundation_Adjustment` Landscape edit layer while keeping the locked `Base_Imported` layer intact. It samples 143 saved wall/gate-join segments, applies a 450 cm corridor with a 500 cm blend, and records the profile in `Data/Canton_WalledCity_Provisional_Wall_Foundation_Profile.json`. The 474 coarse wall actors were replaced by 4,790 collision-enabled modules no longer than 249.8 cm, with endpoint/cross-section terrain sampling and a 140 cm buried base. Gate actors and their saved transforms were not changed.

Validation passed after a fresh editor reload: zero positive bottom gaps, no coarse wall actors, the foundation layer present, and all adaptive modules within the length limit. A fresh Unreal disk-save pass reloaded 9 landmarks and 4,790 adaptive walls with no dirty Canton packages remaining. The scoped two-map cook processed 1,532/1,532 packages without errors, and the staged player-camera check passed at 1280×720 with a 0.9996 nonblack fraction. Closeups were recaptured for all eight gates.

Evidence: [terrain-fit validation](../../QA/Canton_Continuation/Walled_Wall_Terrain_Fit_Validation.json), [disk-save validation](../../QA/Canton_Continuation/Walled_Disk_Save.json), [cook report](../../QA/Canton_Continuation/Delivery_Cook.json), [runtime visibility](../../QA/Canton_Continuation/Walled_Runtime_Visibility_Check.json), and [gate closeups](../../QA/Canton_Continuation/Wall_Gate_Fit/After_Capture.json).

[Diagram](2026-09-30-canton-wall-terrain-fit.svg)
