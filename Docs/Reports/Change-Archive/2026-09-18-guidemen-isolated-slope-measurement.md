# Guidemen — isolated slope measurement, and where the roof question now stands

Date: 2026-09-18.

Follow-up to the roof-shell fix, after the user sent a screenshot suggesting the roof
still reads as a valley. No asset was changed in this pass; it is measurement and
documentation.

[Archived diagram](2026-09-18-guidemen-isolated-slope-measurement.svg)

## Why every earlier attempt failed

Five routes were tried and each was blocked, which is worth recording so it is not
retried:

- **instance AABBs are sign-blind** — a Z range cannot say which end is high;
- **the shell meshes are not world-aligned** — each shell's local `+Z` maps to a world
  horizontal axis, so the instance rotation says nothing about the slope;
- **vertex readback is blocked** — `get_vertex_position` needs a `VertexID` that
  cannot be constructed from Python (the struct's `id` property is not settable), even
  though `get_static_mesh_description(0)` works and reports counts;
- **`HitResult` fields are protected** — a trace's hit location cannot be read, so the
  obvious raycast route fails at the last step;
- **the tile component is shared by both tiers** (`R1_T0` holds 16,248 instances
  across R1 and R2), so hiding it isolates neither.

## What worked

Disable collision on every other roof component, then find the target surface's Z by
**bisecting a line trace on the boolean alone** — 18 iterations resolve a surface to
0.01 cm. No `HitResult` fields, no geometry access. Visibility does not affect traces;
collision does (`set_collision_enabled`).

## Result 1 — the shells are now slope-correct

| shell | samples along its slope |
| --- | --- |
| `R1_RoofShellBack` | (0,30)=1457.92 → (0,130)=1427.32 → (0,230)=1391.88 → (0,330)=1354.81 → (0,430)=1319.75 |
| `R2_RoofShellBack` | (0,30)=1850.75 → (0,430)=1713.89 |
| `R1_RoofShellRight` | (100,0)=1454.22 → (1300,0)=1304.70 |
| `R2_RoofShellRight` | (100,0)=1846.98 → (1300,0)=1708.51 |

Every isolated shell **falls monotonically away from the ridge line** — the Back shells
with `|y|` (main slopes), the Right shells with `|x|` (hip ends). That is a correct
roof, so the shell fix from the previous pass is geometrically sound and is kept.

The Front and Left shells returned no hit because the fix maps each onto its mirror
partner's footprint, so only one of each pair is topmost.

## Result 2 — the tiles disagree with the shells

The tile component's own surface, isolated the same way, along `y` at `x = 0`:

```
y= 30 → 1851.22     y=130 → 1854.45     y=230 → 1851.30
y=330 → 1852.62     y=430 → 1851.74     y=480 → 1855.42
```

The tile surface is **flat** at about 1852 cm along `y`, while the R2 shell beneath it
falls 137 cm over the same span. On the upper tier the tiles do not lie on the shells.

Along `x` the tiles do fall (1851 at `x=0` to 1718 at `x=1200`), which is consistent
with a ridge running along **Y** — but the ridge bars run along **X**.

## The three surfaces do not agree

| surface | reading | implies |
| --- | --- | --- |
| shells | fall with `\|y\|` (main slopes) and `\|x\|` (hip ends) | ridge along **X** |
| tiles | flat along `y` at `x=0`, falling along `x` | ridge along **Y** |
| ridge bars | run along X, 3120 / 2720 cm | ridge along **X** |

Two of the three say the ridge runs along X, which is also what the pavilion's
31.2 × 11.3 m proportion implies. The tile component is the outlier — and it is the one
component that could not be isolated cleanly, because `R1_T0` holds both tiers' tiles.
Its flat-along-`y` reading at `x = 0` is the one measurement in this pass that should
not be fully trusted.

## Where this leaves it

- The shell fix is verified correct by isolation and stays.
- The tiles-versus-shells disagreement on the upper tier is real but unresolved, and is
  the likely source of what the screenshot shows.
- **Two views would settle it in seconds**: a top-down view and a front or side
  elevation. They show the ridge line and each slope's direction directly, which is
  exactly what no measurement available here could do.

## Method note

The capture harness is still broken, so this pass had no visual review either. Every
conclusion above comes from geometry and traces. A user screenshot is currently the
only rendering evidence available on this project — which is why the user, and not the
tooling, has now caught two of the three roof defects found on these assets.
