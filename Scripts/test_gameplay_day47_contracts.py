"""Offline contract checks for the inert Day 47A offer-boundary milestone."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config"
TEST_SOURCE = ROOT / "Source" / "Aura" / "Private" / "Tests"


class GameplayDay47ContractTests(unittest.TestCase):
    def test_scope_lists_exact_eight_architecture_augments(self) -> None:
        scope = json.loads((ROOT / "Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json").read_text(
            encoding="utf-8"
        ))
        self.assertEqual(scope["features"]["augments"], [
            "quick_step", "guarded_step", "steady_hands", "field_medic",
            "forked_spark", "ember_ring", "fast_cycle", "wide_trap",
        ])

    def test_day47a_harness_is_private_test_only(self) -> None:
        expected = (
            TEST_SOURCE / "AuraGameplayDay47Harness.h",
            TEST_SOURCE / "AuraGameplayDay47Harness.cpp",
            TEST_SOURCE / "AuraGameplayDay47Tests.cpp",
        )
        for path in expected:
            self.assertTrue(path.is_file(), path)

        header = expected[0].read_text(encoding="utf-8")
        source = expected[1].read_text(encoding="utf-8")
        native = expected[2].read_text(encoding="utf-8")
        self.assertIn("completion results", header)
        self.assertIn("WITH_DEV_AUTOMATION_TESTS", header)
        self.assertIn("RegisterCellDeath", source)
        self.assertIn("NextDeterministicValue", source)
        self.assertNotIn("AbilitySystem", source)
        self.assertNotIn("InputComponent", source)
        self.assertNotIn("AuraRunSupplyComponent", source)
        self.assertNotIn("GameplayAugmentDefinitions", source)

        expected_tests = {
            "DefinitionCatalog",
            "OfferEligibilityAndUniqueness",
            "DeterministicSeedReplay",
            "BoundaryOffersExactlyTwo",
            "ChoiceReplayNoStack",
            "TimeoutChoosesVisibleDefault",
            "ReconnectKeepsOffer",
            "OwnerPrivacyAndStaleBoundary",
        }
        registered_tests = set(re.findall(r'"Aura\.Gameplay\.Day47A\.([A-Za-z0-9]+)"', native))
        self.assertEqual(registered_tests, expected_tests)

    def test_full_day47_consumers_remain_deferred(self) -> None:
        full_day = (ROOT / "Docs/Plans/Gameplay-Expansion-Implementation/Day-47-Run-Augments.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("Status: Planned", full_day)
        for future_path in (
            CONFIG / "GameplayAugmentDefinitions.json",
            ROOT / "Source/Aura/Public/Gameplay/AuraAugmentOfferTypes.h",
            ROOT / "Source/Aura/Public/Gameplay/AuraRunAugmentComponent.h",
            ROOT / "Source/Aura/Private/Gameplay/AuraRunAugmentComponent.cpp",
        ):
            self.assertFalse(future_path.exists(), future_path)

    def test_runtime_entry_and_day46_dependencies_remain_fail_closed(self) -> None:
        mission_subsystem = (ROOT / "Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("DAY40_PACKAGED_EVIDENCE_MISSING", mission_subsystem)
        self.assertIn("bRuntimeEntryReady = false", mission_subsystem)
        day46 = (ROOT / "Docs/Plans/Gameplay-Expansion-Implementation/Day-46-Role-Kits-and-Combos.md").read_text(
            encoding="utf-8"
        )
        self.assertIn("Status: Planned", day46)
        self.assertIn("AuraRunSupplyComponent", day46)
        self.assertIn("Exposed has one authoritative record per target", day46)


if __name__ == "__main__":
    unittest.main(verbosity=2)
