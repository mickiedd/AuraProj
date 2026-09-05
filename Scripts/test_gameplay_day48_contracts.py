"""Offline contract checks for the inert Day 48 objective reducer foundation."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config" / "GameplayObjectiveDefinitions.json"
NATIVE = ROOT / "Source" / "Aura" / "Private" / "Tests" / "AuraGameplayDay48Tests.cpp"
OBJECTIVE_TYPES = ROOT / "Source" / "Aura" / "Public" / "Gameplay" / "AuraObjectiveTypes.h"
OBJECTIVE_SOURCE = ROOT / "Source" / "Aura" / "Private" / "Gameplay" / "AuraObjectiveTypes.cpp"


class GameplayDay48ContractTests(unittest.TestCase):
    def test_objective_config_is_closed_and_fail_closed(self) -> None:
        document = json.loads(CONFIG.read_text(encoding="utf-8"))
        self.assertEqual(document["schemaVersion"], 1)
        self.assertEqual(document["gameplayProfile"], "GameplayExpansionV1")
        self.assertEqual(document["status"], "CONTRACT_ONLY")
        objectives = document["objectives"]
        self.assertEqual({objective["type"] for objective in objectives}, {"Clear", "Escort", "InteractHold"})
        ids = {objective["id"] for objective in objectives}
        self.assertEqual(len(ids), len(objectives))
        self.assertTrue(all(prerequisite in ids for objective in objectives for prerequisite in objective.get("prerequisiteIds", [])))
        self.assertEqual(objectives[1]["requiredMemberIds"], ["civilian_a", "civilian_b"])
        self.assertEqual(objectives[1]["targetCount"], 2)
        self.assertEqual(objectives[2]["deadlineSeconds"], 180)

    def test_native_reducer_is_authority_only_and_not_wired(self) -> None:
        header = OBJECTIVE_TYPES.read_text(encoding="utf-8")
        source = OBJECTIVE_SOURCE.read_text(encoding="utf-8")
        mission_subsystem = (ROOT / "Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp").read_text(encoding="utf-8")
        self.assertIn("Authority-only ordered objective reducer", header)
        self.assertIn("ForeignObjectiveEvent", source)
        self.assertIn("RequiredObjectiveUnreachable", source)
        self.assertNotIn("AuraObjectiveTypes", mission_subsystem)
        self.assertIn("DAY40_PACKAGED_EVIDENCE_MISSING", mission_subsystem)

    def test_named_native_contract_cases_are_present(self) -> None:
        native = NATIVE.read_text(encoding="utf-8")
        expected = {
            "ForeignOutcomeIgnored",
            "ObjectiveDAGRejected",
            "ChannelRaceSingleOwner",
            "ChannelCancelNoProgress",
            "EscortIdentityRequired",
            "ExtractionRequiresAllPrimary",
            "GeneralizedSequenceKeepsChoices",
        }
        self.assertEqual(set(re.findall(r'"Aura\.Gameplay\.Day48\.([A-Za-z0-9]+)"', native)), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
