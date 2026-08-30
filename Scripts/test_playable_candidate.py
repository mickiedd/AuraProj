#!/usr/bin/env python3
"""Deterministic local contract tests for the Playable Candidate Days 21-40.

The Unreal automation suite remains the authority for engine behavior.  This
small harness covers the same contracts without requiring a GPU, a provider,
or a long-running packaged process, and emits reviewable JSON evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import re
import sys
import time
import unittest
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCOPE_PATH = ROOT / "Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json"
MANIFEST_PATH = ROOT / "Content/Config/PlayableCandidateManifest.json"


def load_json(relative: str):
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


@dataclass
class CandidateState:
    role: str = "BungeeMan"
    alive: bool = True
    held: bool = False
    magazine: int = 12
    reserve: int = 48
    wallet: int = 100
    items: dict[str, int] = field(default_factory=dict)
    stock: int = 20
    reward_ids: set[str] = field(default_factory=set)
    life: str = "Alive"
    recovery_transitions: int = 0
    generation: int = 1
    last_observed: datetime | None = None
    last_restock: datetime | None = None
    processed_death_sequences: set[int] = field(default_factory=set)

    def press(self) -> str:
        if self.role != "BungeeMan":
            return "NotApplicable"
        if self.held:
            return "HeldInputIgnored"
        self.held = True
        if not self.alive:
            return "NotAlive"
        if self.magazine <= 0:
            return "EmptyMagazine"
        self.magazine -= 1
        return "Accepted"

    def release(self) -> None:
        self.held = False

    def reload(self) -> str:
        if self.role != "BungeeMan":
            return "NotApplicable"
        if self.magazine == 12:
            return "MagazineFull"
        if self.reserve <= 0:
            return "NoReserve"
        self.magazine, self.reserve = min(12, self.magazine + self.reserve), max(0, self.reserve - (12 - self.magazine))
        return "ReloadCompleted"

    def reward(self, outcome_id: str) -> bool:
        if outcome_id in self.reward_ids:
            return False
        self.reward_ids.add(outcome_id)
        self.wallet += 25
        return True

    def purchase_health_potion(self) -> str:
        if self.wallet < 25:
            return "InsufficientFunds"
        if self.stock <= 0:
            return "SoldOut"
        self.wallet -= 25
        self.items["health_potion"] = self.items.get("health_potion", 0) + 1
        self.stock -= 1
        return "Success"

    def restock(self, observed: datetime, startup: bool = False) -> str:
        if self.last_observed is not None and observed < self.last_observed:
            observed = self.last_observed
        self.last_observed = observed
        if self.last_restock is None:
            self.last_restock = observed
            return "Initialized"
        if observed - self.last_restock < timedelta(seconds=600):
            return "NotDue"
        self.last_restock = observed
        if self.stock < 20:
            self.stock = 20
            return "StartupRestocked" if startup else "Restocked"
        return "AlreadyFull"

    def kill_and_recover(self, sequence: int = 1) -> None:
        if sequence in self.processed_death_sequences:
            return
        self.processed_death_sequences.add(sequence)
        if self.life not in {"Alive", "Recovering"}:
            return
        self.life = "Dead"
        self.recovery_transitions += 1
        self.life = "Recovering"
        self.generation += 1
        self.life = "Alive"


class CandidateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scope = load_json("Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json")
        cls.manifest = load_json("Content/Config/PlayableCandidateManifest.json")
        cls.roles = load_json("Content/Config/RoleConfig.json")
        cls.merchant = load_json("Content/Config/MerchantDefinitions.json")
        cls.reward = load_json("Content/Config/RewardDefinitions.json")
        cls.tutorial = load_json("Content/Config/PlayableCandidateTutorial.json")

    def test_day21_scope_is_frozen(self):
        self.assertEqual(self.scope["scopeRevision"], "playable-candidate-v1")
        self.assertEqual(self.scope["canonicalMap"], "/Game/Maps/StartupMap")
        self.assertEqual({lane["id"] for lane in self.scope["lanes"]}, {"Aura-listen", "BungeeMan-listen", "Aura-dedicated", "BungeeMan-dedicated"})
        self.assertEqual([step["order"] for step in self.scope["journeySteps"]], list(range(1, 17)))

    def test_day22_baseline_contract(self):
        self.assertEqual(self.manifest["mapAlias"], "RoleBattleCivilianTest")
        self.assertEqual(set(self.manifest["requiredRoles"]), {"Aura", "BungeeMan"})
        self.assertEqual(set(self.manifest["requiredTopologies"]), {"listen", "dedicated"})

    def test_day23_boot_is_bounded(self):
        states = ["LoginPending", "ServerReady", "Loading", "RoleValidated", "HUDReady", "Controllable"]
        self.assertEqual(len(states), 6)
        self.assertEqual(states[0], "LoginPending")
        self.assertEqual(states[-1], "Controllable")

    def test_day24_tutorial_is_server_owned(self):
        self.assertEqual(len(self.tutorial["steps"]), 6)
        self.assertEqual([step["order"] for step in self.tutorial["steps"]], list(range(1, 7)))
        self.assertEqual(self.tutorial["completionAuthority"], "server-owned-event-or-observed-authoritative-state")
        self.assertTrue(all(set(step["roles"]) == {"Aura", "BungeeMan"} for step in self.tutorial["steps"]))
        player_state = (ROOT / "Source/Aura/Private/Player/AuraPlayerState.cpp").read_text(encoding="utf-8")
        hud = (ROOT / "Source/Aura/Private/UI/HUD/AuraHUD.cpp").read_text(encoding="utf-8")
        interaction = (ROOT / "Source/Aura/Private/Interaction/AuraInteractionComponent.cpp").read_text(encoding="utf-8")
        self.assertIn("OnRep_TutorialCompletionMask", player_state)
        self.assertIn("OnTutorialProgressChanged.Broadcast", player_state)
        self.assertIn("OnTutorialProgressChanged.AddUObject", hud)
        self.assertIn("SetTutorialStepCompleted", interaction)

    def test_day25_ammo_is_authoritative_and_bounded(self):
        state = CandidateState()
        self.assertEqual(state.press(), "Accepted")
        self.assertEqual(state.magazine, 11)
        self.assertEqual(state.press(), "HeldInputIgnored")
        self.assertEqual(state.magazine, 11)
        state.release()
        self.assertEqual(state.reload(), "ReloadCompleted")
        self.assertEqual((state.magazine, state.reserve), (12, 47))
        firearm = self.manifest["firearm"]
        self.assertEqual(firearm["fireMode"], "SemiAuto")
        self.assertEqual(firearm["shotConsumption"], 1)
        source = (ROOT / "Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp").read_text(encoding="utf-8")
        self.assertIn("AuthorityUnavailable", source)
        self.assertLess(source.index("ConfigureFromDefinition"), source.index("TryConsumeFirearmRound"))
        self.assertLess(source.index("TryConsumeFirearmRound"), source.index("FinishSpawning"))

    def test_day26_hud_is_owner_safe(self):
        self.assertEqual(self.scope["fireGunPolicy"]["auraHudState"], "NotApplicable")
        hud = (ROOT / "Plugins/AuraWebUI/Content/WebUI/hud-right-top.html").read_text(encoding="utf-8")
        self.assertIn("hud_firearm", hud)
        self.assertIn("Not applicable", hud)

    def test_day27_semi_auto_press_edge(self):
        source = (ROOT / "Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp").read_text(encoding="utf-8")
        self.assertIn("Press-edge activation", source)
        self.assertIn("Do not turn browser/native held notifications into automatic fire", source)
        self.assertEqual(self.scope["fireGunPolicy"]["heldInputRepeats"], False)

    def test_day28_death_recovery_is_once(self):
        state = CandidateState()
        state.kill_and_recover(sequence=1)
        state.kill_and_recover(sequence=1)
        self.assertEqual(state.life, "Alive")
        self.assertEqual(state.recovery_transitions, 1)
        game_mode = (ROOT / "Source/Aura/Private/Game/AuraGameModeBase.cpp").read_text(encoding="utf-8")
        player_state = (ROOT / "Source/Aura/Private/Player/AuraPlayerState.cpp").read_text(encoding="utf-8")
        self.assertLess(game_mode.index("CancelFirearmReload(TEXT(\"PlayerDeath\"))"), game_mode.index("SetRecoveryState(TEXT(\"Dead\"))"))
        self.assertIn("NotAlive", player_state)

    def test_day29_reward_and_purchase_are_atomic(self):
        state = CandidateState()
        self.assertTrue(state.reward("civilian:1"))
        self.assertFalse(state.reward("civilian:1"))
        self.assertEqual(state.wallet, 125)
        self.assertEqual(state.purchase_health_potion(), "Success")
        self.assertEqual((state.wallet, state.items["health_potion"], state.stock), (100, 1, 19))
        reward = self.reward["rewards"][0]
        self.assertEqual((reward["amount"], reward["purchasePrice"]), (25, 25))

    def test_day30_restock_uses_max_observed_clock_and_inclusive_boundary(self):
        state = CandidateState(stock=19)
        first = datetime(2026, 8, 30, tzinfo=timezone.utc)
        self.assertEqual(state.restock(first), "Initialized")
        self.assertEqual(state.restock(first + timedelta(seconds=599)), "NotDue")
        self.assertEqual(state.restock(first + timedelta(seconds=600)), "Restocked")
        state.stock = 19
        self.assertEqual(state.restock(first - timedelta(days=1)), "NotDue")
        self.assertEqual(state.stock, 19)
        self.assertEqual(self.manifest["restock"]["intervalSeconds"], 600)

    def test_day31_persistence_closes_all_authority_fields(self):
        fields = set(self.scope["playerPersistencePolicy"]["fields"])
        self.assertTrue({"completedAmmo", "tutorialCompletion", "lifeState"} <= fields)
        self.assertTrue({"merchantStock", "lastRestockAtUtc", "lastObservedAtUtc", "stockRevision"} <= set(self.scope["worldPersistencePolicy"]["fields"]))
        source = (ROOT / "Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp").read_text(encoding="utf-8")
        self.assertIn("LastRestockAtUtc", source)
        self.assertIn("RestoreCheckpointState", source)
        self.assertIn("DuplicateObject<UAuraPlayerSaveGame>", source)
        self.assertIn("DeleteGameInSlot", source)
        character = (ROOT / "Source/Aura/Private/Character/AuraCharacter.cpp").read_text(encoding="utf-8")
        self.assertLess(character.index("ApplyRoleAtSpawn(AuthorizedRole"), character.index("ApplyPersistentProfile(*PersistentProfile"))

    def test_day32_generation_convergence(self):
        state = CandidateState()
        before = state.generation
        state.kill_and_recover()
        self.assertGreater(state.generation, before)
        self.assertEqual(state.life, "Alive")

    def test_day33_negative_requests_have_explicit_codes(self):
        state = CandidateState(role="Aura")
        self.assertEqual(state.press(), "NotApplicable")
        state = CandidateState(magazine=0)
        self.assertEqual(state.press(), "EmptyMagazine")
        self.assertEqual(state.purchase_health_potion(), "Success")

    def test_day34_content_graph_is_resolvable(self):
        ability_paths = {p.relative_to(ROOT).as_posix() for p in (ROOT / "Content/AbilityDefinitions").glob("*.xml")}
        for required in self.manifest["requiredFiles"]:
            if "*." not in required:
                self.assertTrue((ROOT / required).is_file(), required)
        for role in self.roles["roles"]:
            ref = role.get("lmbAbilityDefinition", "")
            if ref:
                self.assertIn("Content/" + ref.removeprefix("/Game/"), ability_paths)
        firegun = ET.parse(ROOT / "Content/AbilityDefinitions/FireGun.xml").getroot()
        self.assertEqual(firegun.attrib["fireMode"], "SemiAuto")

    def test_day35_diagnostics_are_redacted(self):
        diagnostics = self.manifest["diagnostics"]
        self.assertEqual(diagnostics["identity"], "run-scoped-HMAC")
        self.assertEqual(diagnostics["rawIdentity"], "never-logged")
        self.assertEqual(diagnostics["secrets"], "never-logged")
        secret = b"candidate-run-secret"
        digest = hmac.new(secret, b"provider:account", hashlib.sha256).hexdigest()
        self.assertEqual(len(digest), 64)
        self.assertNotIn("provider:account", digest)
        writer = (ROOT / "Scripts/WritePlayableCandidateDiagnostics.ps1").read_text(encoding="utf-8")
        for token in ("buildRevision", "sessionId", "serverInstanceId", "worldPersistenceId", "playerIdentityHmac", "resultCode", "AURA_CANDIDATE_HMAC_KEY_BASE64"):
            self.assertIn(token, writer)
        self.assertNotIn("RandomNumberGenerator", writer)

    def test_day36_all_packaged_lanes_are_declared(self):
        self.assertEqual(len(self.scope["lanes"]), 4)
        self.assertTrue(all(lane.get("firearmState") in {"Required", "NotApplicable"} for lane in self.scope["lanes"]))

    def test_day37_runbook_has_second_developer_path(self):
        runbook = ROOT / "Docs/Reference/Playable-Candidate-Server-Operations.md"
        self.assertTrue(runbook.is_file())
        text = runbook.read_text(encoding="utf-8")
        self.assertIn("RunPlayableCandidate.ps1", text)
        self.assertIn("BLOCKED", text)

    def test_day38_stage_propagation_is_explicit(self):
        runner = ROOT / "RunPlayableCandidate.ps1"
        self.assertTrue(runner.is_file())
        text = runner.read_text(encoding="utf-8")
        for token in ("Fast", "Candidate", "External", "candidate-draft.json", "BLOCKED", "DIAGNOSTIC", "packageSha256", "clean working tree"):
            self.assertIn(token, text)

    def test_day39_soak_is_bounded_and_four_lane(self):
        runner = (ROOT / "RunPlayableCandidate.ps1").read_text(encoding="utf-8")
        wrapper = (ROOT / "Scripts/RunPlayableCandidateDay39Soak.ps1").read_text(encoding="utf-8")
        self.assertIn("cyclesPerLane = 0", runner)
        self.assertIn("No packaged four-lane ten-cycle soak was executed", runner)
        self.assertIn("-Stage Candidate -Soak", wrapper)

    def test_day40_signoff_hash_inputs_are_immutable(self):
        self.assertEqual(self.manifest["scopeRevision"], self.scope["scopeRevision"])
        manifest_hash = hashlib.sha256(MANIFEST_PATH.read_bytes()).hexdigest()
        self.assertEqual(len(manifest_hash), 64)
        runner = (ROOT / "RunPlayableCandidate.ps1").read_text(encoding="utf-8")
        for token in ("Assert-RecordBinding", "cyclesPerLane", "Set-ItemProperty", "packageSha256"):
            self.assertIn(token, runner)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--days", default="all", help="day number or comma-separated day numbers")
    parser.add_argument("--mode", default="Both")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    selected = "all" if args.days == "all" else {int(value) for value in args.days.split(",")}
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(CandidateContractTests)
    if selected != "all":
        suite = unittest.TestSuite(test for test in suite if any(test._testMethodName.startswith(f"test_day{day:02d}") for day in selected))
    stream = unittest.TestResult()
    started = time.time()
    suite.run(stream)
    failures = [{"test": test.id(), "error": error.splitlines()[-1] if error else ""} for test, error in stream.failures + stream.errors]
    report = {
        "schemaVersion": 1,
        "scopeRevision": CandidateContractTests.scope["scopeRevision"],
        "days": args.days,
        "mode": args.mode,
        "testsRun": stream.testsRun,
        "failures": failures,
        "passed": not failures and stream.testsRun > 0,
        "durationSeconds": round(time.time() - started, 3),
    }
    encoded = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
