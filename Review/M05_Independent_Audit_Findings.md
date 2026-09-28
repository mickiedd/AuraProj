# M05 Canton provisional district — independent audit findings

**Audience:** the implementing agent (Codex). This is a **review hand-off**, not a project
instruction: nothing here lowers an acceptance threshold or asserts a historical measurement.

**Reviewed:** `Review/M05_Final_Handoff.md` @ `14041340a781dc8bbb417c6f36a9dd922b56a023`,
its machine-readable freeze, the 605-entry district manifest, and the two levels the cook and
handoff claim: `L_Canton_District_PROVISIONAL` and `L_Canton_WalledCity_PROVISIONAL`.

**Method:** read-only. No Unreal process was opened, so nothing in the frozen package was
rewritten. Every claim below is re-derived from the R16 source, the map-building scripts, the
validator source and the retained evidence — see
[`Scripts/audit_canton_district_handoff.py`](../Scripts/audit_canton_district_handoff.py) and the
machine-readable companion [`M05_Independent_Audit_Findings.json`](M05_Independent_Audit_Findings.json).

**Verdict: 0 blockers, 8 major, 4 minor.** The delivery is honest about its open historical and
runtime gates and I could not falsify a single headline number it reports. The defects are in
**what the package does and does not prove about itself**, and one of them explains the gate
failure that is currently costing Day 14.

---

## 1. What I verified as sound — do not redo this

| Check | Result |
|---|---|
| District manifest hashes | **605 / 605 matched at audit time**, 0 missing, 0 size or SHA-256 mismatches. The archive entry this job is required to write then moved exactly one row — see AUD-FREEZE-002, which is about the freeze design, not the package |
| Repository freeze | `base_commit` equals live `HEAD`; manifest and handoff SHA-256 both current; 219 dirty entries recorded |
| Cook config restoration | `Config/DefaultGame.ini` is **byte-identical to the `HEAD` blob** (`706697544ed995c8…`); exactly four `DirectoriesToAlwaysCook` entries exist, matching the declared exclusion |
| Route-matrix arithmetic | `route_count`, `route_check_pass_count`, `all_route_checks_pass` and every `nav_detour_ratio` recompute exactly from the raw path lengths |
| R16 decode and orientation | 2017² samples / 8,136,578 B; `(code−32768)·50/128` agrees with `metres_per_code 0.00390625`; south-first row order is the one the C++ decoder and the profile script both assume; contract `raw_sha256` matches the file |
| Drainage fall | all three catchments are nonincreasing at 4 m steps; worst grades 5.664 % (principal), 7.617 % (mixed), 5.371 % (gutter) |
| Wetness A/B pair | identical camera and target, damp toggle the only declared difference, 38,915 changed pixels (2.70 %) |
| Handoff links | every relative link resolves |
| Scope language | the handoff restates the budget's own 33.3 ms / 50 ms / 12 GiB targets and never claims historical acceptance |

---

## 2. Major findings

### AUD-FREEZE-002 — the freeze hashes an append-only file, so it cannot stay valid

`Data/M02_M05_District_Artifacts.csv` includes `.claude/memory/visual-change-archive.md`. AGENTS.md
requires **every** completed job to append an entry to that index. So the freeze invalidates itself
as soon as the next job records itself — and this audit, doing exactly what policy demands, moved
that row and nothing else. The two rules cannot both be satisfied: a compliant archive entry always
breaks the freeze.

**Requested revision:** drop the append-only index from the manifest and freeze something stable
about it instead — its line count plus the last line's hash, or the tail commit. Then re-freeze.

### AUD-GATE-001 — the gate route fails because the road runs through a 65 m closed gate

This is the headline, and it is now diagnosed rather than open.

`gate_threshold_approach` runs `(180000,120500) → (180000,123500)`. `BP_Wenmingmen` is spawned at
`(180000, 121500)`, i.e. **across that corridor**. Three measurements agree:

- The gate's declared footprint is **65 × 35 m** (`asset_manifest.json`); the placed Blueprint
  measures **6500.0 × 2806.9 × 2199.0 cm** (package `README_UE5.md`).
- The navigation detour's furthest-east point is **x = 183293 cm — 3293 cm east of the road
  centre**. The gate's east face is at 3250 cm, and the UE default nav agent radius is 35 cm:
  **3250 + 35 = 3285 cm**. The path is hugging the gate's outer wall to within 8 cm.
- The gate package models a **closed, recessed double timber door shaped to the arch**, and the
  import uses **complex collision on structural meshes** — the same meshes that produce the
  288,064 / 595,104 triangle navigation-export warnings.

So the 2.583× detour is not a collision defect to "simplify": **the corridor is architecturally
blocked**, and the route's own start point `(180000,120500)` sits *inside* the gate footprint.
The query cannot be satisfied while the doors are shut.

**Requested revision:** choose an intent and record it — move the gate clear of the corridor;
open a passage through it; end the route *at* the threshold instead of 20 m beyond it; or waive
directness with an owner note. Re-running the same unsatisfiable query is not a diagnosis.
Whichever you pick, `CANTON-M05-004` should stop reading as "diagnose gate opening and collision"
and start naming the chosen resolution.

### AUD-COVERAGE-001 — the second cooked level is not in the frozen manifest

`Cook_Result.json` cooks two maps and the handoff cites both, but `Content/Canton/Provisional/**`
— the entire `L_Canton_WalledCity_PROVISIONAL` package — **appears zero times** in
`Data/M02_M05_District_Artifacts.csv`. So does its evidence: `Review/M01_Terrain_Review.md`,
`QA/UE_Import_Screenshots/Automation_Validation.json` and the M01 screenshots are all outside the
freeze.

The freeze therefore cannot reproduce or verify one of the two levels the delivery claims.
`Automation_Validation.json` (27 Sep 09:19) is the *only* current pass for that level — 256
components, 256 collision components, 17 actors, max collision error 0.0157 cm — and it is not
hashed by anything.

**Requested revision:** extend `refresh_canton_district_manifest.py` to every cooked level and its
evidence, then re-freeze; or scope the cook to the district and stop citing the walled-city level
as delivered.

### AUD-EVIDENCE-001 — three "distinct" evidence items are one byte-identical artifact

`Reload_Validation.json`, `Seam_Probe.json` and `PreNav_Validation.json` are **the same file**:
sha256 `53f3ce189d84a425…`, byte for byte. That is expected — `ProbeCantonDistrictSeams.py`,
`BuildCantonDistrictNavigation.py` and `ReviewCantonDistrictPrototype.py` all call the same
`UCantonTerrainLibrary::ValidateDistrictWorld` — but the handoff's evidence index lists
"[Native seam probe]" as the direct evidence for *Geometric seams*, which reads as an independent
check. It is not one, and it cannot be distinguished from the reload validation by a reader.

**Requested revision:** cite the validator once, and if a distinct seam claim is wanted, make a
distinct measurement (e.g. slab-to-slab joint step across the longitudinal direction, which
nothing currently measures).

### AUD-VIEW-001 — two different images carry the same view name

`Review/M05_QA_Screenshots/gate_approach.png` (904,950 B) and `QA/Canton_District/gate_approach.png`
(904,976 B) **differ in 428,010 bytes** — genuinely different renders, not re-compression. The
handoff links the older (27 Sep) copy while `Capture_Settings.json` records the newer (28 Sep)
run. The other six views are identical between the two directories, so only the gate view is
ambiguous — and it is the one view that matters most right now.

**Requested revision:** decide which capture is authoritative, replace the stale copy, re-freeze.

### AUD-NAVROAD-001 — the navigation surface does not sit on the authored road

The road top is a **constant** build-up above the R16 — `surface_offset 7 + height 12 = z_at+19`
for the principal road, `5 + 9 = z_at+14` for the mixed lane. The navigation mesh is not:

| Route | Surface | nav − R16 | designed road top | off the road top |
|---|---|---:|---:|---:|
| principal_gate_side_to_interior | principal | +19.16 | +19 | **+0.16** |
| mixed_lane_west_to_junction | mixed | +19.01 | +14 | +5.01 |
| mixed_lane_junction_to_east | mixed | +34.64 | +14 | **+20.64** |
| principal_to_mixed_intersection | intersection | +30.79 | +19 | **+11.79** |
| gate_threshold_approach | gate | +29.39 | +19 | **+10.39** |
| covered_gutter_crossing | gutter | +28.61 | +19 | +9.61 |
| mixed_lane_to_courtyard_reference | reference_only | +18.38 | +14 | +4.38 |

A 16.3 cm spread along routes whose road top is constant means the nav mesh is riding on terrain,
shoulder, slab edge or a quantisation step — not reliably on the road. Part of this is Recast
cell-height quantisation and is benign; **nothing in the package bounds it, and no check relates
the nav mesh to the road at all.** Day 14's own "done when" is continuous traversal, so this is
the wrong thing to leave unmeasured.

**Requested revision:** add a nav-to-road check, or record the quantisation budget that explains
the spread, before Day 14 is closed.

### AUD-BUILD-001 — the frozen `Build_Result.json` describes a superseded build

`instance_counts.pebble = 96` and `instance_counts.plot = 16`, but the map has **76** mixed-lane
slabs and **0** parcel pads (`Reload_Validation.json`), and the current
`BuildCantonDistrictPrototype.py` no longer places parcel pads at all. The file is hashed into the
manifest as evidence, so the package ships two different district compositions and a reader cannot
tell which one is the delivery.

**Requested revision:** regenerate `Build_Result.json` from the current script, or label it
explicitly as the initial-build snapshot with the refinement that superseded it.

### AUD-THRESHOLD-001 — numeric gates are enforced in code but declared nowhere

`Docs/Terrain_Acceptance_Budget.md` and the 20-day plan contain **no** navigation-directness gate
and **no** numeric road-grade gate. Yet:

- `detour_ratio <= 1.5` fails a route in `ReviewCantonDistrictTraversal.py`,
  `run_canton_district_check.py` and `test_canton_district_contract.py`;
- `step_slope_percent < 8` is asserted by the contract test, and `Slope_Refinement.json` records
  that **20 mixed-lane pieces were deleted** because the raster grade exceeded **12 %**.

So geometry was changed to satisfy a threshold that no versioned budget declares. The handoff's
"No acceptance threshold was lowered" is true and beside the point: two thresholds were
*introduced* without being versioned or approved, and one of them altered the built map.

**Requested revision:** declare both gates in the versioned budget with owner approval, or drop
them from pass/fail and report the numbers as diagnostics.

---

## 3. Minor findings

| ID | Finding | Requested revision |
|---|---|---|
| AUD-VALIDATOR-001 | `ValidateDistrictWorld` accepts `MixedSlabs 70..80` (now 76), `Gutters >= 45` (now 48) and `Weeds > 0 && Damp > 0`. Removing six lane pieces or three gutters still returns `passed: true`. | Assert exact counts or ±2 so the check can actually fail. |
| AUD-SLAB-001 | The principal slab spans `z_at+7` (underside) to `z_at+19` (top) — a 7 cm gap over the terrain — and the shoulder top is `z_at+8`, an **11 cm step at the road edge**. The long-edge check compares top edges to terrain and can see neither. | Close the underside with a kerb/skirt, or record the lip as an accepted blockout property and add a view that shows it. |
| AUD-WAIVE-001 | Day 14 requires every relevant map warning to be **fixed or explicitly waived**. The two navigation-export warnings (288,064 / 595,104 triangles) are neither — they are carried as an open issue. | Add an explicit waiver with owner and follow-up, or fix the collision export. |
| AUD-NANITE-001 | The handoff lists "Nanite compatibility NOT RUN". The gate package records that on **Metal SM5 Nanite is unavailable** and meshes use raster fallback — and the acceptance budget names this same Mac as the proposed target hardware. The subcheck is *unavailable*, not pending. | State that the host cannot evaluate the gate, and either change the target hardware or declare the gate inapplicable with owner approval. |

---

## 4. Reproduce this audit

```sh
python3 Scripts/audit_canton_district_handoff.py     # rewrites Review/M05_Independent_Audit_Findings.json
```

Read-only, no Unreal, no writes into the frozen package. Re-run it after the revisions and the
counts in the JSON should fall to zero; the same script is the regression test for this hand-off.

## 5. What this audit did not cover

No Unreal process was opened, so nothing here re-measures the map, the collision, the navigation
build or the cook. The gate diagnosis in AUD-GATE-001 is derived from the placed actor's
coordinates, the package's own declared bounds and the recorded detour apex — it is arithmetic on
recorded numbers, not a viewport observation. Confirming it needs one fresh-load run with the
gate's collision drawn or the doors opened. Runtime performance, World Partition streaming, pawn
traversal and every historical axis remain exactly as blocked as the handoff states.
