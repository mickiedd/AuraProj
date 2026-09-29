# M03 material and paving QA — technical blockout

The isolated Landscape now has a compiled source-neutral material with six named paint targets and saved layer-info assets: Soil, Earth, Pebble, Grass, Damp and Stone. Unpainted ground falls back to neutral soil color. Eight simple ground/road materials distinguish the principal road, mixed lane, shoulder, gutter and damp-detail blockout. Their flat colors are not sampled from monochrome photographs and carry no claim of exact late-Qing surface color; `Data/Material_Evidence_Register.csv` marks them D-confidence.

The principal road uses repeated 2 m cube-based slab modules following the terrain by bilinear R16 sampling. Native checks establish collision and edge-height continuity, but the visible regular joints and basic cube geometry are not accepted historical stone modules. Kerbs, authentic slab dimensions, texel density, normal/detail treatment and source-backed arrangement remain open. The four fixed-camera surfaces are shown in [the provisional material atlas](M03_Material_Atlas.png): main street, mixed lane, gate approach and courtyard edge. The courtyard is a reference edge, not a built historic parcel platform.

**Decision:** material-layer infrastructure and the traversable modular blockout pass; final material and paving art remain provisional. The historical and final-art gaps are recorded in `Data/Open_Issues.csv`.

## Continuation — 2026-09-29 (current candidate)

100 principal modules use generated beveled paving geometry (540 triangles per source module), grounded skirts and the existing simple collision envelope. Six parameterized material instances and six physical materials replace swatches; project-authored stone, earth and pebble textures provide scale-aware color and normal detail. Vertical support faces use a separate world projection to avoid stretched vertical texture columns. Six Landscape paint targets are retained, and Base_Imported remains locked/unmodified.

The refreshed atlas and seven current captures were visually inspected: the raised approach and gate foundation are supported, paving joints are visible, and no fallback shader appears. The procedural inputs and licence/provenance are frozen in `ContentSource/CantonDistrict/manifest.json`. Shapes, dimensions and palette are D-confidence interpretation. This implements the engineering surfaces; source-backed period appearance and final art approval remain open under CANTON-M03-001. The courtyard view remains open-ground reference, not a constructed courtyard.
