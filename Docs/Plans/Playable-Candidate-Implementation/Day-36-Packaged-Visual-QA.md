# Day 36 — Packaged Visual Resolution QA

Status: Planned  
Depends on: Days 23–28 and Day 32

## Goal

Close the gap between passing logic tests and a visibly usable packaged game.

## Work

- Declare the mandatory profiles as 1280x720 at 100% DPI, 1920x1080 at 100% DPI, and 2560x1440 at 125% DPI; fullscreen/windowed behavior is recorded for each profile. A profile in the manifest is a required candidate gate, not an optional sample.
- Create a deterministic screenshot checklist for login, loading, role HUD, combat/ammo, targeting, tutorial, merchant, death/recovery, reconnect, and late join.
- Verify the three bounded WebUI panels do not clip, overlap, block world input, or lose replayed state.
- Record unsupported display configurations honestly instead of silently accepting them. A declared profile that cannot be exercised is `BLOCKED` and prevents visual PASS; only a profile excluded from the manifest before the run may be `NotApplicable`.

## Detailed execution contract

### Files to inspect or modify

- **Packaged surface:** Shipping client package, `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`, project input settings, and the package/staging manifest.
- **WebUI/layout:** `Plugins/AuraWebUI/Content/WebUI/login.html`, `loading.html`, `index.html`, `hud-left-top.html`, `hud-right-top.html`, `hud-bottom.html`, and the bridge/widget layout code.
- **Native input/state:** `Source/Aura/Public/UI/HUD/AuraHUD.h/.cpp`, `AuraPlayerController`, viewport/input routing, and replicated presentation state.
- **New output:** `Source/Aura/Private/Tests/AuraRoleBattleDay36Tests.cpp`, `RunPlayableCandidateDay36VisualQA.ps1`, `day-36-visual.json`, and deterministic captures.

### Visual contract

The mandatory profiles are 1280x720 at 100% DPI, 1920x1080 at 100% DPI, and 2560x1440 at 125% DPI, each tested windowed and fullscreen where supported. Capture points are login, loading, role HUD, combat/ammo, targeting, tutorial, merchant, death/recovery, reconnect, and late join. Each row records readable/interactable/privacy/input status and package revision.

### Detailed steps

1. Build/package the Shipping client from the Day 35/38 artifact and record package hash before opening the first profile.
2. Set each supported resolution/DPI/window mode and capture the declared deterministic checkpoints; do not accept editor/PIE captures as packaged proof.
3. Check panel bounds, font/readability, clipping/overlap, alpha/background, scroll/overflow, focus/pointer capture, keyboard/mouse input, and native world input around every panel.
4. Verify HUD state after initial ready, target change, accepted/rejected attack, BungeeMan ammo empty/reload, Aura firearm `NotApplicable`, hit/kill, death/recovery, merchant, save, late join, and reconnect.
5. Run both Aura and BungeeMan and a second-client privacy check; confirm the second client cannot see private wallet/inventory/ammo fields.
6. Record unsupported configurations explicitly as `BLOCKED` when they are declared profiles; do not let an unexercised mandatory profile disappear from PASS counts. Profiles outside the candidate manifest are recorded as `NotApplicable` only.
7. Re-run one failure case (invalid resolution or missing staged WebUI asset) and confirm it produces an actionable report without a false visual PASS.

### Named automation and evidence

- Run `RunPlayableCandidateDay36VisualQA.ps1 -Profile ...` for each declared profile and retain PNG/capture metadata, logs, build revision, package hash, and checklist row.
- A profile fails on any clipping, overlap, input-blocking, unreadable state, stale replay, privacy leak, or missing asset. No screenshot-diff platform is required.

## Validation and evidence

- Run the matrix against packaged Shipping client builds, not only PIE.
- Retain screenshots, resolution/DPI metadata, build revision, and pass/fail notes.
- Include a manual visual check for Aura and BungeeMan and a second-client privacy check.

## Deep-review closure

- **Owner surfaces:** the packaged Shipping client, `Plugins/AuraWebUI/Content/WebUI/`, WebUI bridge/layout code, and the input/viewport boundary.
- **Required artifacts:** `day-36-visual.json`, one capture/checklist row per declared profile and player state, DPI/resolution metadata, and a second-client privacy result.
- **Gate:** every declared profile renders and accepts the required interaction with zero known clipping, overlap, input-blocking, or state-replay defects; an unavailable declared profile is `BLOCKED`, while only an undeclared profile may be listed as unsupported without affecting PASS.

## Completion gate

All declared supported visual states are readable and interactable in packaged builds, with zero known clipping or input-blocking regressions.

## Defer

Do not build computer-vision screenshot comparison; deterministic capture points and a checklist are sufficient.
