# Crunch Num.4 Tornado implementation

Date: 2026-09-06

## Intent and diagnosis

Make Crunch's fourth numbered skill produce its intended spin and bounded area damage. The user's `Saved/Logs/Aura.log` confirms `InputTag.4` activation accepted and server-confirmed around 09:20:43–09:20:48 UTC. Load warnings at lines 1632 and 1636 identify missing `Anim_Turnado` and hook particle dependencies in the legacy montage. Input was reaching the ability.

## Changed behavior

- Imported the original `Anim_Turnado` from `C:/Works/Crunch-master` at its original package path as generation source. A dedicated `AuraCreateTornadoMontage` commandlet duplicates it onto the installed `Crunch_SkeletonV4`, removes legacy notifies, disables root motion, and creates `AM_Tornado_Aura` with one DefaultSlot and a four-second duration. No Blueprint graph was edited.
- Tornado uses the repaired montage through GAS, scaled to the configured cast duration. Its server-owned persistent `GameplayCue.Crunch.Tornado` creates an attached existing `NS_Tornado` Niagara effect. Both OnActive and WhileActive are handled idempotently; Removed destroys the component. Dedicated servers skip rendering. The server clears the scoped prediction key for cue delivery to the predicting owner.
- Only the authority's repeating timer owns hit cadence. Removed the generic damage-event listener, eliminating a second source of hit indices. Existing defaults remain: four seconds, 0.1-second interval, 300-unit radius, 20 base physical damage and 3000-unit outward push. Combat rules reject self, friendly, dying/dead and out-of-range targets. No mana or cooldown change.
- Removed Tornado's inherited random debuff chance: an initial runtime test proved this could continue damaging a target after the cast. This skill now has direct hits only.
- Timers are registered before montage task activation to cover synchronous cancellation. Every exit clears gameplay timers and the persistent cue; GAS ends the montage task, stopping its playback. Logs record start parameters, authority, timer count, end and per-victim hits at Verbose level.

## Validation

- Build: `C:/Git/UE_5.5/Engine/Build/BatchFiles/Build.bat AuraEditor Win64 DebugGame C:/Git/AuraProj/Aura.uproject -waitmutex` — PASS. A `-NoUBTMakefiles` build was used once to discover the new test translation unit. The user's latest session was DebugGame.
- Generation: `UnrealEditor-Win64-DebugGame-Cmd.exe Aura.uproject -run=AuraCreateTornadoMontage -unattended -nullrhi -nosound` — saved the animation and montage, length 4.000 seconds, skeleton Crunch_SkeletonV4. The first generation process logged the expected missing new montage at CDO construction before creating it; subsequent validation loads the saved asset successfully.
- Suite: `UnrealEditor-Win64-DebugGame-Cmd.exe Aura.uproject -unattended -RenderOffscreen -nosound "-ExecCmds=Automation RunTests Aura.Migration.Crunch.;Quit" "-TestExit=Automation Test Queue Empty"` — all 11 tests passed, process exit 0. Report: `Saved/Reports/CrunchTornado/index.json`; log: `Saved/Logs/CrunchTornadoTests.log`. Offscreen startup also reports the pre-existing editor ToolMenus registration error; no test failures.
- New `Aura.Migration.Crunch.Tornado.DamagePresentationAndCleanup` loads the real spin, verifies authored bone tracks and skeleton, plays the actual montage on SM_CrunchV4 with ABP_Crunch_AuraV5, checks enemy damage and protected targets, timeout/cancellation/recast, no late damage, persistent cue ownership, native Niagara creation, duplicate activation and removal.
- Existing numbered press-edge, Dash travel/collision/cancel, GroundBlast, role, identity and presentation tests also passed.
- Final headless focused Tornado test also passed (exit 0): `Saved/Reports/CrunchTornadoHeadless/index.json`. The test explicitly permits Niagara rendering to be skipped under NullRHI.
- `git diff --check` — PASS. The SVG was rendered to PNG and visually checked for readability.

## Review and limitations

The required `in-app-claude-handoff` skill was attempted at its configured Google AI Studio URL. Tab creation timed out; state inspection confirmed the tab existed; reacquisition timed out. No packet was typed or sent. External review was unavailable; the documented fallback used local implementation/UE engine review, compiled builds and executable regression tests. No independent external approval is claimed.

This is an automated transient-world runtime validation with offscreen rendering, not a live physical-key, dedicated-server owner/observer or visual appearance playtest. Niagara component creation and animation playback are verified; appearance and remote replication remain to be checked in normal play. Restart the DebugGame editor/game to load the new native binary and assets. Existing unrelated worktree changes were preserved.

## Illustration

[Before/after flow and validation](2026-09-06-crunch-num4-tornado.svg)

