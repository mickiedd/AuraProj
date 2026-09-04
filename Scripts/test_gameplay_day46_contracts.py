"""Offline contract checks for the inert Day 46A cross-contract harness."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config"
TEST_SOURCE = ROOT / "Source" / "Aura" / "Private" / "Tests"


class GameplayDay46ContractTests(unittest.TestCase):
    def test_existing_definition_sources_remain_parseable_and_closed(self) -> None:
        definition_names = (
            "GameplayMissionDefinitions.json",
            "GameplayRoleLoadouts.json",
            "GameplayEncounterDefinitions.json",
            "GameplayEnemyArchetypes.json",
            "GameplayArenaLayouts.json",
        )
        documents = {}
        for name in definition_names:
            path = CONFIG / name
            self.assertTrue(path.is_file(), path)
            documents[name] = json.loads(path.read_text(encoding="utf-8"))

        revisions = {document["definitionRevision"] for document in documents.values()}
        self.assertEqual(revisions, {"days-42-60-foundation-v1"})
        self.assertEqual({document["schemaVersion"] for document in documents.values()}, {1})
        self.assertEqual({document["gameplayProfile"] for document in documents.values()}, {"GameplayExpansionV1"})

        registry = (ROOT / "Source/Aura/Private/Gameplay/AuraGameplayDefinitionRegistry.cpp").read_text(
            encoding="utf-8"
        )
        for name in definition_names:
            self.assertIn(name, registry)
        self.assertEqual(registry.count("LoadJsonObject(Paths["), 5)

    def test_day46a_contract_harness_is_test_only(self) -> None:
        expected = (
            TEST_SOURCE / "AuraGameplayDay46Harness.h",
            TEST_SOURCE / "AuraGameplayDay46Harness.cpp",
            TEST_SOURCE / "AuraGameplayDay46Tests.cpp",
        )
        for path in expected:
            self.assertTrue(path.is_file(), path)

        harness_header = expected[0].read_text(encoding="utf-8")
        harness_source = expected[1].read_text(encoding="utf-8")
        native_tests = expected[2].read_text(encoding="utf-8")
        self.assertIn("Stable, value-only projection", harness_header)
        self.assertIn("same value snapshot", native_tests)
        self.assertIn("ExecutePrimaryScenario", harness_source)
        self.assertNotIn("FPlatformTime", harness_source)
        self.assertNotIn("AbilitySystem", harness_source)
        self.assertNotIn("CharacterMovement", harness_source)
        self.assertNotIn("InputComponent", harness_source)

        expected_tests = {
            "DeterministicReplay",
            "EncounterGenerationBarrier",
            "SourceLifeBarrier",
            "DriverOwnershipBarrier",
            "HeavyLeaseLifecycleConvergence",
            "SupportOwnerPromotion",
            "AttackReplacementIsolation",
            "TerminalCleanupIdempotence",
            "CrossContractStaleStorm",
        }
        registered_tests = set(re.findall(r'"Aura\.Gameplay\.Day46\.([A-Za-z0-9]+)"', native_tests))
        self.assertEqual(registered_tests, expected_tests)

    def test_full_role_kit_day_remains_deferred(self) -> None:
        full_day = (ROOT / "Docs/Plans/Gameplay-Expansion-Implementation/Day-46-Role-Kits-and-Combos.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("Status: Planned", full_day)
        supply = json.loads((CONFIG / "GameplaySupplyDefinitions.json").read_text(encoding="utf-8"))
        self.assertEqual(supply["status"], "CONTRACT_ONLY")
        self.assertEqual(supply["runtimeWiring"], "UNWIRED")
        for future_path in (
            ROOT / "Content/AbilityDefinitions/FocusShot.xml",
            ROOT / "Content/AbilityDefinitions/ShockTrap.xml",
            ROOT / "Source/Aura/Public/Gameplay/AuraRunSupplyComponent.h",
        ):
            self.assertFalse(future_path.exists(), future_path)

    def test_runtime_entry_and_evidence_gates_remain_fail_closed(self) -> None:
        mission_subsystem = (ROOT / "Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("DAY40_PACKAGED_EVIDENCE_MISSING", mission_subsystem)
        self.assertIn("bRuntimeEntryReady = false", mission_subsystem)

        layouts = json.loads((CONFIG / "GameplayArenaLayouts.json").read_text(encoding="utf-8"))
        self.assertEqual(layouts["surveyStatus"], "BLOCKED")
        self.assertEqual(layouts["blocker"], "ANCHOR_SURVEY_UNVERIFIED")
        self.assertTrue(all(not layout["verified"] for layout in layouts["layouts"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
