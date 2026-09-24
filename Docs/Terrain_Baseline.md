# Canton terrain baseline — 2026-09-24

**State: provisional planning contract; historical production is blocked.** The study period is 1880–1900. The area of interest is the Qing walled city and a working buffer; its polygon is not yet measured. No historical wall, gate or elevation coordinates are approved.

## Coordinate and asset contract

- GIS horizontal frame: WGS 84 / UTM zone 49N, EPSG:32649, metres. Guangzhou is within the CRS's 108°–114° E area of use ([EPSG definition](https://epsg.io/32649)). This is a working projection, not evidence that the old map has been georeferenced.
- Local UE axes: X east, Y north, Z up; 100 UE centimetres = 1 GIS metre. The projected XY origin and terrain-zero elevation remain **unset** until independently checked ground control and a vertical datum are available. See `Data/Coordinate_Contract.json`.
- Proposed Landscape only: 2017² vertices at 2 m spacing, about 4032 m square; 63 quads/section, 2 × 2 sections/component, 16 × 16 components. Size and placement must be rechecked against the measured wall and buffer before import. Z scale 50 is conditional on an audited ±128 m encoding.
- Prefix prospective terrain packages `Canton_`; keep source IDs in GIS feature properties. Gate mesh size or pivot is asset geometry, never a surveyed ground height. `ContentSource/GuangzhouLandmarks/Wenmingmen/unreal-import-manifest.json` currently reports a 6500 × 2806.9 × 2199.0 cm imported bounding box; it is not a coordinate control.

## Reproducibility and storage

Baseline repository revision: `c840ce2a22993c33491a5d0fa1c74add125cf3ce`. Existing unrelated Wenmingmen and editor changes were present before this job and are preserved. Host Python is 3.9.6. The project GUID association resolves through `Scripts/macos/unreal-common.sh` to `/Volumes/M2/Engine/UE_5.5`; `Engine/Build/Build.version` reports UE 5.5.4, changelist 40574608.

Keep small, licensed source scans in `Sources/` with checksum and source-register row. Keep original large GeoTIFFs and derived rasters outside Git or in Git LFS after checking repository size policy; record URI, checksum and generating command in `Data/Artifact_Manifest.csv`. `.uasset` and `.umap` already use Git LFS. Never silently resample or overwrite an original. Preserve each tool version and source revision in the relevant resource sheet.

The 1890-labelled Vrooman scan in `Sources/HistoricMaps/` is only a candidate. Its [Commons record](https://commons.wikimedia.org/wiki/File:Canton1890.jpg) says 1890 while embedded British Library metadata says *The Canton Guide*, second edition, 1880. Archive record BL `001954731`; exact plate and map revision need confirmation. It cannot serve as accepted georeferencing input yet.

## Day gates

The five resource sheets record their current status. Day 03 requires independently located persistent controls and holdouts; Day 04 requires an accepted georeferenced image; Day 05 requires suitable bare-earth or surveyed elevation evidence and a reconciled vertical datum. The current map and elevation candidates do not waive those requirements.
