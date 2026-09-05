#!/usr/bin/env python3
"""Static checks for the inert Day 50 encounter-pacing contract milestone."""
from __future__ import annotations

import json
import re
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content/Config/GameplayPacingDefinitions.json"
HEADER = ROOT / "Source/Aura/Public/Gameplay/AuraEncounterPacingTypes.h"
SOURCE = ROOT / "Source/Aura/Private/Gameplay/AuraEncounterPacingTypes.cpp"
NATIVE = ROOT / "Source/Aura/Private/Tests/AuraGameplayDay50Tests.cpp"
MISSION = ROOT / "Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp"
COORDINATOR = ROOT / "Source/Aura/Private/Gameplay/AuraEncounterCoordinator.cpp"


class GameplayDay50ContractTests(unittest.TestCase):
    def test_pacing_definition_is_closed_and_fail_closed(self) -> None:
        data = json.loads(CONFIG.read_text(encoding="utf-8"))
        self.assertEqual(data["schemaVersion"], 1)
        self.assertEqual(data["gameplayProfile"], "GameplayExpansionV1")
        self.assertEqual(data["status"], "CONTRACT_ONLY")
        self.assertEqual(data["runtimeWiring"], "UNWIRED")
        self.assertEqual(data["schedulerHz"], 2)
        self.assertEqual(data["intensity"]["recoveryEnterThreshold"], 0.75)
        self.assertEqual(data["intensity"]["recoveryExitThreshold"], 0.45)
        self.assertEqual(data["phases"], {
            "minimumBuildSeconds": 10,
            "maximumPressureSeconds": 30,
            "minimumRecoverySeconds": 8,
            "maximumRecoverySeconds": 20,
            "interactionMayExtendRecovery": True,
        })
        self.assertEqual(data["admission"]["archetypeCosts"], {
            "Raider": 1, "Lancer": 2, "Bulwark": 3, "Disruptor": 3,
        })
        self.assertEqual(data["admission"]["waveBudget"], {"solo": 6, "pair": 10})
        self.assertEqual(data["admission"]["activeHostileCap"], {"solo": 10, "pair": 16})
        self.assertEqual(data["heavyAdmission"]["owner"], "AuraEncounterCoordinator")
        self.assertFalse(data["heavyAdmission"]["pacingMayResetTokens"])
        self.assertIn("ANCHOR_SURVEY_UNVERIFIED", data["runtimeGates"])

    def test_reducer_is_authority_only_and_unwired(self) -> None:
        header = HEADER.read_text(encoding="utf-8")
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("Inert Day 50 authority reducer", header)
        self.assertIn("PacingTickRejected", source)
        self.assertIn("RequiredRosterDeadlineExpired", source)
        self.assertNotIn("AuraEncounterPacingTypes", MISSION.read_text(encoding="utf-8"))
        self.assertNotIn("AuraEncounterPacingTypes", COORDINATOR.read_text(encoding="utf-8"))
        self.assertNotIn("UAuraEncounterPacingPolicy", NATIVE.read_text(encoding="utf-8"))

    def test_all_named_native_cases_are_present(self) -> None:
        expected = {
            "HysteresisPreventsChatter",
            "BudgetAndLiveCaps",
            "RecoverySuppressesNewSpawns",
            "RequiredRosterStillCompletes",
            "DisconnectNoHealthRewrite",
            "HeavyBudgetOwnerPreserved",
            "SeedAndInputsReproduceDecisions",
        }
        native = NATIVE.read_text(encoding="utf-8")
        self.assertEqual(set(re.findall(r'"Aura\.Gameplay\.Day50\.([A-Za-z0-9]+)"', native)), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
