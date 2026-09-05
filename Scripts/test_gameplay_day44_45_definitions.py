"""Offline contract checks for the inert Days 44-45 gameplay foundation."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config"


def load_config(filename: str) -> dict:
    return json.loads((CONFIG / filename).read_text(encoding="utf-8"))


class GameplayDay44DefinitionsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.combat = load_config("GameplayCombatTuning.json")
        cls.archetypes = load_config("GameplayEnemyArchetypes.json")
        cls.encounters = load_config("GameplayEncounterDefinitions.json")
        cls.layouts = load_config("GameplayArenaLayouts.json")

    def test_day44_attack_timing_schema_is_strict(self) -> None:
        timeline = self.combat["attackTimeline"]
        self.assertEqual(timeline["minimumWindupSeconds"], 0.85)
        self.assertEqual(timeline["minimumPreRelevantCueLeadSeconds"], 0.65)
        self.assertEqual(timeline["preRelevantSampleCount"], 100)
        self.assertEqual(timeline["preRelevantPassCount"], 95)
        self.assertEqual(timeline["cueVisibilityMetric"], "FirstVisuallyConfirmedRenderedFrame")
        self.assertEqual(timeline["lateRelevanceReporting"], "SeparateCohort")
        self.assertFalse(timeline["cancelledAttackerMayResolveImpact"])

    def test_day44_evade_constants_are_contractual(self) -> None:
        evade = self.combat["evade"]
        self.assertEqual(evade["maxDistanceUnits"], 350.0)
        self.assertEqual(evade["durationSeconds"], 0.25)
        self.assertEqual(evade["cooldownSeconds"], 2.5)
        self.assertEqual(evade["maxCharges"], 1)
        self.assertEqual(evade["collisionResolution"], "AuthoritativeSwept")
        self.assertEqual(evade["directionPlane"], "Horizontal")
        self.assertFalse(evade["grantsInvulnerability"])
        self.assertEqual(evade["acceptedCancellation"], ["OwnedInteraction", "Reload", "OwnedChannel"])

    def test_day44_shape_and_telegraph_references_are_closed(self) -> None:
        shapes = {shape["id"] for shape in self.combat["attackShapes"]}
        profiles = {profile["id"] for profile in self.combat["telegraphProfiles"]}
        self.assertEqual(len(shapes), len(self.combat["attackShapes"]))
        self.assertEqual(len(profiles), len(self.combat["telegraphProfiles"]))
        self.assertTrue(all(profile["shapeId"] in shapes for profile in self.combat["telegraphProfiles"]))
        for archetype in self.archetypes["archetypes"]:
            for attack in archetype["attacks"]:
                self.assertIn(attack["attackShapeId"], shapes)
                self.assertIn(attack["telegraphProfileId"], profiles)


class GameplayDay45DefinitionsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.combat = load_config("GameplayCombatTuning.json")
        cls.archetypes = load_config("GameplayEnemyArchetypes.json")
        cls.encounters = load_config("GameplayEncounterDefinitions.json")
        cls.layouts = load_config("GameplayArenaLayouts.json")

    def test_day45_required_archetypes_are_unique_and_resolvable(self) -> None:
        archetypes = self.archetypes["archetypes"]
        by_id = {archetype["id"]: archetype for archetype in archetypes}
        self.assertEqual(set(by_id), {"Raider", "Lancer", "Bulwark", "Disruptor"})
        for archetype in archetypes:
            self.assertTrue(archetype["attacks"])
            self.assertEqual(len({attack["id"] for attack in archetype["attacks"]}), len(archetype["attacks"]))
            for attack in archetype["attacks"]:
                self.assertGreaterEqual(attack["windupSeconds"], 0.85)
                self.assertTrue(attack["consumesHeavyAdmission"])

    def test_day45_counter_contracts_are_frozen(self) -> None:
        by_id = {archetype["id"]: archetype for archetype in self.archetypes["archetypes"]}
        self.assertTrue(by_id["Lancer"]["attacks"][0]["shapeFrozenDuringWindup"])
        bulwark = by_id["Bulwark"]
        self.assertEqual(bulwark["frontArcDegrees"], 120.0)
        self.assertEqual(bulwark["frontDamageReductionFraction"], 0.60)
        self.assertEqual(bulwark["slamRecoverySeconds"], 1.2)
        disruptor = by_id["Disruptor"]
        support = disruptor["supportField"]
        self.assertEqual(support["durationSeconds"], 2.0)
        self.assertEqual(support["damageReductionFraction"], 0.20)
        self.assertEqual(support["rangeUnits"], 600.0)
        self.assertEqual(support["maxAllies"], 2)
        self.assertEqual(support["ownershipPolicy"], "LowestLiveSourceLease")
        self.assertTrue(support["interruptible"])
        self.assertTrue(support["removeOnRangeDeparture"])
        self.assertTrue(support["removeOnOwnerDeath"])

    def test_day45_heavy_budget_is_valid(self) -> None:
        policy = self.encounters["policy"]
        self.assertEqual(policy["maxConcurrentHeavyAttacks"], 2)
        self.assertEqual(policy["maxHeavyTokensPerParticipant"], 2)
        self.assertEqual(policy["heavyCoverageRangeUnits"], 600.0)
        self.assertEqual(policy["maxDisruptors"], 2)
        self.assertEqual(policy["supportFieldOwnershipPolicy"], "LowestLiveSourceLease")

    def test_day45_does_not_require_arena_transforms(self) -> None:
        for archetype in self.archetypes["archetypes"]:
            self.assertNotIn("transform", archetype)
            self.assertNotIn("location", archetype)
        for encounter in self.encounters["encounters"]:
            self.assertNotIn("transform", encounter)
            self.assertNotIn("location", encounter)

    def test_day45_does_not_clear_anchor_survey_blocker(self) -> None:
        self.assertEqual(self.layouts["surveyStatus"], "BLOCKED")
        self.assertEqual(self.layouts["blocker"], "ANCHOR_SURVEY_UNVERIFIED")
        self.assertTrue(all(not layout["verified"] for layout in self.layouts["layouts"]))


class GameplayDay44And45SourceContractsTests(unittest.TestCase):
    def test_native_contract_files_exist_and_are_unwired(self) -> None:
        expected = (
            ROOT / "Source/Aura/Public/Gameplay/AuraAttackTimelineTypes.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraAttackTimelineTypes.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraAttackTimelineComponent.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraAttackTimelineComponent.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraEvadeTypes.h",
            ROOT / "Source/Aura/Public/Gameplay/AuraEvadePolicy.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraEvadePolicy.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraEvadeComponent.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraEvadeComponent.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraEnemyArchetypeTypes.h",
            ROOT / "Source/Aura/Public/Gameplay/AuraEncounterCoordinator.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraEncounterCoordinator.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraCombatStatusTypes.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraCombatStatusTypes.cpp",
            ROOT / "Source/Aura/Public/Gameplay/AuraCombatStatusComponent.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraCombatStatusComponent.cpp",
            ROOT / "Source/Aura/Private/Tests/AuraGameplayDay44Tests.cpp",
            ROOT / "Source/Aura/Private/Tests/AuraGameplayDay45Tests.cpp",
        )
        for path in expected:
            self.assertTrue(path.exists(), path)

        controller = (ROOT / "Source/Aura/Private/Player/AuraPlayerController.cpp").read_text(encoding="utf-8")
        self.assertNotIn("ServerRequestEvade", controller)
        mission_subsystem = (ROOT / "Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("DAY40_PACKAGED_EVIDENCE_MISSING", mission_subsystem)
        self.assertIn("bRuntimeEntryReady = false", mission_subsystem)

    def test_handoff_regressions_have_explicit_native_coverage(self) -> None:
        coordinator = (ROOT / "Source/Aura/Private/Gameplay/AuraEncounterCoordinator.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("FindActiveHeavyLeaseIndex", coordinator)
        self.assertIn("FindActiveSupportFieldLeaseIndex", coordinator)
        self.assertIn("MakeHeavyLeaseFingerprint", coordinator)
        self.assertIn("MakeSupportLeaseFingerprint", coordinator)

        timeline_types = (ROOT / "Source/Aura/Private/Gameplay/AuraAttackTimelineTypes.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("if (Phase != EAuraAttackTimelinePhase::Cancelled) return;", timeline_types)

        day44_tests = (ROOT / "Source/Aura/Private/Tests/AuraGameplayDay44Tests.cpp").read_text(
            encoding="utf-8"
        )
        day45_tests = (ROOT / "Source/Aura/Private/Tests/AuraGameplayDay45Tests.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("CancelledTimelineRequiresExplicitReset", day44_tests)
        self.assertIn("ActiveLeaseReplaySurvivesHistoryEviction", day45_tests)

    def test_existing_registry_stays_five_source_and_fail_closed(self) -> None:
        registry = (ROOT / "Source/Aura/Private/Gameplay/AuraGameplayDefinitionRegistry.cpp").read_text(
            encoding="utf-8"
        )
        for filename in (
            "GameplayMissionDefinitions.json",
            "GameplayRoleLoadouts.json",
            "GameplayEncounterDefinitions.json",
            "GameplayEnemyArchetypes.json",
            "GameplayArenaLayouts.json",
        ):
            self.assertIn(filename, registry)
        self.assertEqual(registry.count("LoadJsonObject(Paths["), 5)
        self.assertIn("ANCHOR_SURVEY_UNVERIFIED", registry)


if __name__ == "__main__":
    unittest.main(verbosity=2)
