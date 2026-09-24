# Wenmingmen material set v2

Four reference-guided material sources were generated with the built-in imagegen tool from the supplied Wenming Gate poster, then processed into 2048 × 2048 PBR maps. The poster is visual guidance; it is not a measured survey. Each generated surface is continuous because the mesh provides the brick edges, carved stone boundaries, roof tile shapes, and door construction.

| Material | Intended use | Albedo character | Base roughness |
| --- | --- | --- | ---: |
| `stone` | individually modeled gray wall masonry | gritty blue-gray stone, mineral flecks, no painted mortar | 0.86 |
| `limestone` | arch voussoirs, bridge rail, coping and plaques | warm pale carved stone, pores and tooling | 0.82 |
| `roof` | individually modeled roof tiles | charcoal blue-gray fired clay, no painted tile rows | 0.75 |
| `wood` | door leaves, brackets, beams and lattice | dark brown aged timber with vertical grain | 0.73 |

For each material, `sources/<name>_generated.png` is the original 1254 × 1254 imagegen output. The 2048 × 2048 processed maps live in the sibling `textures/` directory under the existing Unreal asset stems. They are not duplicated in this v2 package. ORM channels are R = ambient occlusion, G = roughness, B = metallic (zero). The derived normal maps use local image contrast as an *approximation* of microrelief; they are not photogrammetric height data. A narrow edge blend makes opposing albedo edges match for repeat UVs. `material_preview.jpg` shows the four BaseColor maps in stone, limestone, roof, wood order.

## Rebuild and install

From the repository root:

```sh
uv run --with pillow --with numpy python ContentSource/GuangzhouLandmarks/Wenmingmen/materials_v2/generate_maps.py --install
```

`--install` writes directly to the existing asset stems expected by the Unreal importer: 2048 × 2048 `<name>_BaseColor.jpg`, `<name>_Normal.png`, and `<name>_ORM.png` in `ContentSource/GuangzhouLandmarks/Wenmingmen/textures/`. The BaseColor JPEGs use quality 95 with no chroma subsampling. Use `--output-dir /path/to/temporary/dir` to regenerate install-format maps elsewhere for verification. Omit both options to regenerate lossless maps in this v2 directory if needed. Build the GLB with `build_wenmingmen.py --reuse-textures` after installation, so the builder embeds these maps rather than overwriting them with the older procedural textures.

## Generation prompts

These prompts were used with the supplied poster attached as a **visual reference**, not as an edit target. They describe the final selected sources; an early grid-painted masonry study was discarded because the modeled wall already has physical joints.

- **Stone:** Square orthographic, evenly lit, nearly seamless continuous gray-blue weathered stone surface for individually modeled Wenming Gate bricks. Detailed granite/fired-stone grain, subdued mineral flecks, tiny pits, chips, scratches, and lime dust; medium dark-gray value. No masonry pattern, block edges, mortar, joints, architecture, shadows, labels, or text.
- **Limestone:** Square orthographic, evenly lit, nearly seamless pale warm-gray dressed limestone surface matching arch voussoirs, bridge parapet, coping, and plaques. Fine grain, sedimentary veins, pores, subtle chisel marks and age staining. No block seams, carving, architecture, shadows, labels, or text.
- **Roof:** Square orthographic, evenly lit, nearly seamless charcoal blue-gray aged fired-clay ceramic swatch for individually modeled roof tiles. Subtle pores, mineral speckle, worn dusty marks and restrained soot. No tile outlines, rows, mortar, architecture, shadows, labels, or text.
- **Wood:** Square orthographic, evenly lit, nearly seamless dark umber aged hardwood swatch for doors, brackets, beams and lattice. Vertical fibrous grain, worn lacquer, fine grooves and small knots. No plank seams, hardware, frames, architecture, shadows, labels, or text.

## Validation

- All 12 installed maps are 2048 × 2048 RGB images.
- The four BaseColor sets have exact opposing edge pixels before JPEG encoding (mean normalized edge difference: 0.000000 along both axes).
- All four ORM maps have metallic channel 0 and plausible nonmetallic roughness ranges.
- The four-up albedo preview and the wood normal map were inspected visually.
- Regenerating to a temporary directory from the four retained source PNGs reproduced all 12 installed files byte for byte (SHA-256 comparison).
