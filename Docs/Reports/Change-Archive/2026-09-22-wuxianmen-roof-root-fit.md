# Wuxianmen roof root and corner fit

Date: 2026-09-22. [Visual summary](2026-09-22-wuxianmen-roof-root-fit.svg) · [NE capture](2026-09-22-wuxianmen-roof-root-fit-ne.png) · [End capture](2026-09-22-wuxianmen-roof-root-fit-end.png) · [Front capture](2026-09-22-wuxianmen-roof-root-fit-front.png).

## Scope and intent

Continue the reference tuning of `/Game/Assets/Environment/GuangzhouLandmarks/V5/Wuxianmen_FullPBR/BP_Wuxianmen_V5_FullPBR` using the supplied Wuxianmen reference sheet and the user’s roof-corner screenshots. The close views showed the ridge caps reading as floating above the tiled crest and the curved corner ornaments starting away from the gable fascia.

## Changed behavior

- Lowered both retained ridge-cap instances by 20 cm so their rounded lower edge seats into the corresponding tile crest. The saved centers are Z 1188 cm and 1692 cm.
- Generated and imported the private `SM_ClosedGablesAndFinials_RoofFit20260922` mesh. Its eight finial roots now start inside the fascia at the roof corners, with the curved tile ornaments continuing outward from the seated root.
- Rebound `Reference_ClosedGablesAndFinials_GEN_VARIABLE` to the new private mesh and kept the original component mobility, visibility, collision and transform contract.

## Validation

- `roof-fit-geometry-validation.json`: all eight finial root points coincide with the new tile roots and remain within 7.22 cm of the gable fascia; the ridge centers and mesh parts are finite and non-degenerate.
- `roof-fit-live-validation.json`: the respawned Blueprint runtime matches the saved mesh/material/instance bindings.
- `roof-fit-fresh-validation.json`: fresh NullRHI serialized reload passes with full Nanite fallback on the new private mesh.
- `roof-fit-explicit-save.json`: Blueprint and private roof-fit mesh were explicitly saved to local `.uasset` files.
- NE, end and front SceneCapture images were inspected after respawning the saved Blueprint. No map was saved.

Evidence packet and scripts: `C:/Users/mickie/.codex/visualizations/2026/09/21/01a0c4cc-cb1b-78b1-bd83-f6c5a46f6e08/wuxianmen`.
