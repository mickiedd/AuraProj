# Guidemen rebuild — the capture harness is fixed, and what it revealed

Date: 2026-09-18.

The rebuild is now driven by pictures instead of geometry alone. This records the harness
fix, the contamination it exposed, the material rebuild, and the one defect still open.

[Archived diagram](2026-09-18-guidemen-rebuild-capture-and-materials.svg)

## The capture harness is fixed

Five passes of work shipped without a single visual check because the deferred high-res
screenshot renders from the editor viewport, which in this environment writes the same stale
frame for every view. That is now bypassed entirely:

```
SceneCapture2D  ->  persistent TextureRenderTarget2D
                ->  RenderingLibrary.read_render_target(world, rt, True)   [Array[Color]]
                ->  binary PPM (P6) written from the editor
                ->  PNG converted outside the editor
```

Three things had to be learned to make it work:

- **`export_render_target` writes nothing in this build**, even with a persistent target —
  which is why the earlier attempt at this route produced no file. `read_render_target`
  flushes rendering commands internally and returns the whole target as sRGB colours.
- **The editor's Python has no numpy and no PIL.** Pixels must be written as a raw format
  (PPM) and converted by the local interpreter.
- **`set_visibility(False)` does not affect traces or captures; collision and the capture
  component do.**

Verified: successive views now differ (hero mean 0.806, front 0.779, side 0.789, top 0.885),
where before every view was byte-similar.

## What the harness immediately exposed: two buildings in the level

Every capture was showing **two overlapping Guidemen actors** — a stale
`BP_Guidemen_V5_4K_C_0` left behind by an earlier staged setup, plus the current one. The
level held 16 actors at one point. This contaminated every image and would have made any
material or geometry diagnosis wrong.

Cleaned: all 16 stray transient actors removed, level now holds zero.

## Materials rebuilt

With a working camera the first real defect was visible: most of the building rendered
**saturated blue with visible relief** — the classic appearance of a normal map read as base
colour. Only the two materials rebuilt in the earlier pass (stone, tile) rendered correctly,
which was the tell.

Rebuilt all ten source materials with the wiring the packing requires:

```
BaseColor            -> BaseColor   (sRGB)
Normal               -> Normal      (non-sRGB)
MetallicRoughness.G  -> Roughness
MetallicRoughness.B  -> Metallic
AO.R                 -> Ambient Occlusion
```

New `*_Rebuild20260918` siblings, **47 slots rebound**, old graphs left in place for
rollback. The flat-colour materials (dark interior, faded red wood) get their tint with no
sampler.

The source maps were checked first and are all correct: `M_AgedTimber_BaseColor.jpg` is brown
(0.397, 0.249, 0.157), the tiles grey (0.236, 0.243, 0.232), and every `_Normal.jpg` is a
proper (0.498, 0.498, 0.99) normal map. The imported UE textures are correct too — BaseColor
`TC_DEFAULT` + sRGB on, Normal `TC_NORMALMAP` + sRGB off, MetallicRoughness and AO
`TC_MASKS`, at 4096² and 2048².

## The roof reads correctly from above

The top-down view shows a clean hip-roof plan: the ridge line along X with the four hip lines
running diagonally to the corners. **The X/bowtie pattern is the hip lines, not a valley** —
which is exactly what a 庑殿 roof looks like from directly overhead. Combined with the
earlier measurement that every shell falls monotonically away from the ridge, the roof
geometry is consistent.

## Still open

The timber, columns and gate doors still render **saturated blue** with a single actor and
fully rebuilt materials. Ruled out so far:

| ruled out | how |
| --- | --- |
| normal map wired as base colour | all ten materials rebuilt with correct wiring |
| wrong or mislabelled source maps | source JPGs measured: correct albedo, correct normals |
| mis-imported textures | sRGB flags, compression settings and sizes all correct |
| a stale overlapping actor | level cleaned to zero actors, defect persists |
| the material at all | a flat mid-grey material on all 47 slots left the blue elements blue |

That last row is the contradiction worth chasing: with flat grey everywhere, the stone, roof
and crenellations went grey while the timber stayed blue — which points at those *meshes*
rather than their materials, most likely vertex colour or a per-mesh override.

**The next diagnostic is now cheap**: with the harness working, capture each component group
in isolation (hide by collision), which will name the blue geometry outright instead of
inferring it.

## Status

- Capture harness: **fixed**, reusable.
- Level: cleaned.
- Materials: rebuilt, 47 slots rebound, rollback graphs intact.
- Roof: reads correctly from above; shells corrected; tiles match the source to the centimetre.
- Assembly: verified a faithful copy of the source (18,820 of 18,822 instances within 1 cm).
- Recovery copy: `BP_Guidemen_V5_4K_PreRebuild_20260918`.
- **Open:** the blue timber/doors, now isolatable.
