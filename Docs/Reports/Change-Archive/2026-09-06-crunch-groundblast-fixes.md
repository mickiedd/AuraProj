# Crunch GroundBlast activation and presentation fixes

Date: 2026-09-06

## Intent and behavior

Repair the observed Num.3 execution: the prior client logs proved activation acceptance, but the legacy montages referenced a missing Paragon `Cast` animation, held input restarted completed skills, and the blast cue had no production handler.

- Numbered slots 1–4 now activate on the press edge only. Held callbacks cannot retry them; a later press can activate again.
- GroundBlast defaults to a new `AM_GroundBlast_Aura` montage built from installed `Ability_Combo_04V4` on `Crunch_SkeletonV4`, using one DefaultSlot and no copied montage notifies. This is an available animation substitute, not a recovery of the missing original Paragon cast.
- The immediate-cast path no longer loads the broken targeting montage. It checks activity after optional montage task activation, rejecting synchronous cancellation before starting target acquisition.
- The authority checks target payload shape, retraces/validates the point, commits once, and logs the accepted center/radius/victim count. Rejections, timeout and end have explicit logs.
- Production characters implement the GAS cue interface and handle `GameplayCue.Crunch.GroundBlast` with `NS_GroundSummon`. A blank scoped prediction key ensures the authority-only cue also reaches the predicting owner. Dedicated servers skip Niagara rendering.
- The authority plays the cast with the GAS montage task and keeps the ability alive through completion, with a two-second recovery ceiling. This replaces raw server-only montage playback followed by instant end.

The aim-circle/LMB-confirm/RMB-cancel interaction in the broader Num.3 plan is still future work. This repair retains the currently implemented immediate cursor cast. Damage timing remains at validated commit, not at a new animation notify.

## Validation

1. `C:/Git/UE_5.5/Engine/Build/BatchFiles/Build.bat AuraEditor Win64 DebugGame C:/Git/AuraProj/Aura.uproject -waitmutex` — PASS.
2. `UnrealEditor-Win64-DebugGame-Cmd.exe Aura.uproject -run=AuraCreateGroundBlastMontage -unattended -nullrhi -nosound` — replacement asset saved; length 0.933 seconds. Generator explicitly clears the montage constructor's default tracks before adding the cast segment.
3. DebugGame commandlet run with `-ExecCmds="Automation RunTests Aura.Migration.Crunch.;Quit" -TestExit="Automation Test Queue Empty"` — 7 succeeded, 0 failed. Report: `Saved/Reports/CrunchGroundBlast/fixes/index.json`.
4. The numbered-input test now exercises a real ASC in a transient world: press activates, cancellation simulates completion, 30 held frames cannot restart, release/press activates again, for all four numbered slots. The presentation test loads the actual montage/animation dependency, checks slot and skeleton agreement, and checks the production cue handler and Niagara asset.
5. Dedicated server on test port 7797 plus rendered client `?PlayerName=BlastFixProbe?Role=Crunch`. Clicking hotbar 3 produced one authority commit with zero victims at `(800,-858,0.410)`, a visible ground effect on the client, and normal server/client end about 0.93 seconds later. This explicitly verifies visible feedback for an empty valid cast. Logs: `Saved/Logs/GroundBlastServerFix.log`, `Saved/Logs/GroundBlastClientFix.log`.
6. `git diff --check` — PASS.

Physical-key validation remains pending: the computer-use helper's number-row 3 injections did not produce input logs; an injected W also produced no visible movement. Hotbar clicks did reach the same ASC press-edge path. The user was asked to press the physical key in the isolated client. Do not interpret the automated hotbar test as proof of physical keyboard input, victim damage, or observer-client coverage.

## Review

The required `in-app-claude-handoff` workflow was attempted. Creating the Claude tab timed out, state inspection confirmed the tab existed, and reacquiring its contents timed out. No packet was typed or sent. External review is unavailable; the documented fallback used local code/engine review, a compiled build, runtime input/asset tests and the dedicated-server visual exercise. Unrelated BungeeMan changes were preserved.

## Visual evidence

- [Before/after flow](2026-09-06-crunch-groundblast-fixes.svg)
- [Crunch idle before the visual exercise](2026-09-06-crunch-groundblast-fixes-evidence/idle.png)
- [Visible replicated GroundBlast effect](2026-09-06-crunch-groundblast-fixes-evidence/blast.png)
