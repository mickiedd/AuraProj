#!/usr/bin/env python3
"""Static checks for the inert Day 49 escort contract milestone."""
from __future__ import annotations

import json
import re
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config" / "GameplayEscortDefinitions.json"
NATIVE = ROOT / "Source" / "Aura" / "Private" / "Tests" / "AuraGameplayDay49Tests.cpp"
REDUCER = ROOT / "Source" / "Aura" / "Public" / "Gameplay" / "AuraEscortTypes.h"
MISSION_SUBSYSTEM = ROOT / "Source" / "Aura" / "Private" / "Gameplay" / "AuraMissionSubsystem.cpp"
POPULATION_MANAGER = ROOT / "Source" / "Aura" / "Private" / "World" / "AuraPopulationManager.cpp"


class GameplayDay49ContractTests(unittest.TestCase):
    def test_escort_config_is_closed_and_fail_closed(self) -> None:
        with CONFIG.open(encoding="utf-8") as handle:
            data = json.load(handle)
        self.assertEqual(data["schemaVersion"], 1)
        self.assertEqual(data["missionId"], "RescueRelay")
        self.assertEqual(data["status"], "CONTRACT_ONLY")
        self.assertEqual(data["runtimeWiring"], "UNWIRED")
        self.assertEqual(data["requiredMemberIds"], ["civilian_rescue_a", "civilian_rescue_b"])
        self.assertEqual(data["followRangeUnits"], 800)
        self.assertEqual(data["stallPolicy"]["technicalAbortRequestedMovementSeconds"], 9)
        self.assertFalse(data["arrangements"][0]["verified"])
        self.assertFalse(data["arrangements"][1]["verified"])
        self.assertEqual({item["id"] for item in data["arrangements"]}, {"arrangement_a", "arrangement_b"})

    def test_native_escort_reducer_is_test_only_and_not_wired(self) -> None:
        native = NATIVE.read_text(encoding="utf-8")
        reducer = REDUCER.read_text(encoding="utf-8")
        mission = MISSION_SUBSYSTEM.read_text(encoding="utf-8")
        population = POPULATION_MANAGER.read_text(encoding="utf-8")
        self.assertTrue(REDUCER.exists())
        self.assertIn("FAuraEscortReservationLedger", reducer)
        self.assertIn("FAuraEscortRunState", reducer)
        self.assertNotIn("AuraEscortTypes", mission)
        self.assertNotIn("AuraEscortTypes", population)
        self.assertNotIn("UAuraEscortObjectiveComponent", native)

    def test_named_native_contract_cases_are_present(self) -> None:
        native = NATIVE.read_text(encoding="utf-8")
        expected = {
            "ReservationPreventsTwoOwners",
            "EscortPolicyScoped",
            "EscortArrivalExactlyOnce",
            "StallAbortBounded",
            "IntentionalEscortWaitIsNotStall",
            "ReleaseRestoresCivilianWork",
            "LayoutsReachableByBothRoles",
        }
        self.assertEqual(
            set(re.findall(r'"Aura\.Gameplay\.Day49\.([A-Za-z0-9]+)"', native)),
            expected,
        )


if __name__ == "__main__":
    unittest.main()
