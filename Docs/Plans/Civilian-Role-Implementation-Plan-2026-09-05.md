# Civilian as the Fourth Role - Implementation Plan

Date: 2026-09-05  
Status: Implemented and locally validated; revised after Login and locomotion QA

## Scope and decision

AuraProj already contained the runtime Civilian slice: `RoleConfig.json`
published four roles (`Aura`, `Crunch`, `BungeeMan`, and `Civilian`), and the
Civilian actor, population, AI, lifecycle, interaction, and economy paths were
present. The Login gap was that the fourth definition was intentionally marked
non-selectable even though the requested product behavior requires a player to
choose it.

Civilian is now a bounded dual-context role. Population actors continue to use
the ambient `AAuraCivilian` shell and population lifecycle; a connected player
uses the normal `AAuraCharacter` shell with the same server-owned,
non-attacking Civilian identity and the same empty loadout. The shared role ID
is selectable, but no arbitrary ambient role becomes selectable.

The locomotion follow-up found a separate presentation defect: the Civilian
Shaman Anim Blueprint inherited the enemy locomotion graph, but its
`Blueprint Update Animation` event and parent-update call were disabled. The
graph therefore never refreshed `GroundSpeed`, leaving `BS_IdleWalk` at its idle
sample while the actor moved. The authored Anim Blueprint is now enabled and
covered by a native graph assertion.

## Change surfaces and contracts

1. `Content/Config/RoleConfig.json` is the authoritative catalog. It contains
   exactly these stable IDs: `Aura`, `Crunch`, `BungeeMan`, `Civilian`.
   Civilian retains `Entity.AmbientNPC`, `Control.CivilianAI`,
   `Combat.Civilian`, `Faction.Civilian`, `Death.PopulationRespawn`,
   `playerSelectable: true`, `canAttack: false`, `canBeDamaged: true`, and an
   empty offensive/loadout definition.
2. The role parser and `URoleInfo::IsPlayerRoleSelectable` permit only the
   normal Player identity or the bounded Civilian identity to be selectable.
   Civilian remains non-attacking and cannot acquire an offensive grant.
3. `AAuraCivilian` remains the ambient population actor. Authority applies the
   validated Civilian definition once, initializes its replicated ASC and
   AttributeSet, and starts its AI behavior. Population validation remains
   strict for the ambient shell.
4. `AAuraCharacter` is also an approved shell for the exact Civilian identity.
   Its role application is still authority-only; it receives no weapon or LMB
   ability. A Civilian player uses normal player respawn, while an ambient
   `AAuraCivilian` death still refills a population slot.
5. `ALoginPlayerController` continues to publish only roles accepted by the
   shared validator. Because Civilian now passes that validator, the Login
   dropdown must show Aura, BungeeMan, Crunch, and Civilian.

## Numbered implementation steps

1. Confirm the existing four-role catalog and preserve unrelated dirty Crunch
   migration changes.
2. Mark Civilian selectable while retaining its ambient identity and empty
   offensive loadout; permit only this bounded dual-context identity through
   the role parser and shared validator.
3. Add an actor-shell compatibility hook so `AAuraCharacter` can host Civilian
   without weakening the ambient `AAuraCivilian` shell contract.
4. Route Civilian death by authoritative actor shell: population civilians
   refill slots, while player-controlled civilians use player respawn.
5. Update the exact-catalog, load-screen, and Day 8 identity tests plus the fast
   source/config gate.
6. Enable the Shaman Anim Blueprint's inherited `Blueprint Update Animation`
   event and parent call using the editor commandlet; add a focused native test
   that fails if either node is missing or disabled.
7. Run Python contract checks, compile the affected Unreal targets, run the
   focused native suites, cook/package the client, and launch the packaged
   Login screen for runtime startup QA.
8. Record the before/after flow, guard, tests, and any handoff or visual-capture limitation in a
   new dated visual change archive; earlier records remain immutable.

## Verification commands and evidence

Fast gates:

```powershell
python Scripts/test_role_battle_days_7_9.py
python Scripts/test_civilian_ai.py
python Scripts/test_scifi_desert_civilian_population.py
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -run=AuraConfigureCivilianAnimBlueprint -unattended -nop4 -nullrhi
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Civilian.Presentation.LocomotionGraph; Quit'
git diff --check
```

Unreal gate, when `UE_ENGINE_ROOT` and the generated targets are available:

```powershell
& '.\build_test.bat'
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day5; Automation RunTests Aura.RoleBattle.Day8; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/CivilianRoleAutomation.log'
```

Expected evidence is a passing fast-contract output, a commandlet report with
`updateEventEnabled: true` and `parentCallEnabled: true`, a passing focused
native automation log/report, a clean diff check, a packaged Login runtime log
showing roles=4 and `WebUI/login.html`, and the dated SVG/Markdown archive pair.
Any malformed role catalog, missing role, wrong Civilian identity, offensive
grant, unauthorized ambient-role selection, missing locomotion node, or
disabled locomotion node fails the gate; no partial role registry or partial
animation graph is accepted.

## Dependencies, failure semantics, and boundaries

- Depends on the existing version-2 role parser, `AAuraCharacter`,
  `AAuraCivilian`, population manager, and Day 8-15 tests.
- Depends on the existing `ABP_Enemy` locomotion contract: its
  `Blueprint Update Animation` path computes `GroundSpeed`, and its
  `IdleWalkRun` state consumes `BS_IdleWalk`, which contains the Shaman idle
  and walk sequences. The fix is intentionally limited to the Civilian Shaman
  child Anim Blueprint.
- The handoff skill was attempted against the in-app Claude tab. The tab opened,
  but two bounded browser-state reads timed out before a composer became
  available, so no private repository packet was transmitted and no external
  review result is claimed. Local source/config/test validation is the fallback.
- Player-controlled Civilian remains non-attacking and keeps the existing
  protected civilian combat policy. This change does not add weapons, abilities,
  or a new avatar asset.
- A screenshot-based visual check could not be completed in this session: the
  available computer-use surface exposed no native game window after the
  package launch. The packaged Login runtime log and the native Anim Blueprint
  graph test are retained as the fallback evidence; no screenshot claim is
  made.
- Role hot-swapping, civilian offensive abilities, and a second civilian avatar
  type remain explicitly deferred.

## Completion gate

The catalog has exactly four stable roles, Civilian is visible and selectable
through Login, both the player and ambient actor-shell contracts remain
server-validated, the existing Civilian population path remains covered, the
Shaman locomotion update path is enabled and regression-tested, the fast checks,
package check, and diff check pass, and the visual archive exists. The task is
complete only when these artifacts are present and the pre-existing dirty
worktree changes remain untouched.
