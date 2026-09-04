"""Contract checks for the inert Days 42-43 gameplay definition foundation."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content" / "Config"
DEFINITION_FILES = (
    "GameplayMissionDefinitions.json",
    "GameplayRoleLoadouts.json",
    "GameplayEncounterDefinitions.json",
    "GameplayEnemyArchetypes.json",
    "GameplayArenaLayouts.json",
)


def load_definition(filename: str) -> dict:
    return json.loads((CONFIG / filename).read_text(encoding="utf-8"))


class GameplayDay42DefinitionsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.documents = {name: load_definition(name) for name in DEFINITION_FILES}

    def test_all_five_sources_are_explicitly_owned(self) -> None:
        registry = (ROOT / "Source/Aura/Private/Gameplay/AuraGameplayDefinitionRegistry.cpp").read_text(
            encoding="utf-8"
        )
        for filename in DEFINITION_FILES:
            self.assertIn(filename, registry)
        self.assertEqual(registry.count("LoadJsonObject(Paths["), 5)

    def test_shared_headers_and_unique_ids(self) -> None:
        revisions = {document["definitionRevision"] for document in self.documents.values()}
        self.assertEqual(revisions, {"days-42-60-foundation-v1"})
        self.assertEqual({document["schemaVersion"] for document in self.documents.values()}, {1})
        self.assertEqual({document["gameplayProfile"] for document in self.documents.values()}, {"GameplayExpansionV1"})

        roles = self.documents["GameplayRoleLoadouts.json"]["roles"]
        self.assertEqual(len({role["id"] for role in roles}), len(roles))
        for role in roles:
            self.assertGreaterEqual(len(role["abilityIds"]), 1)
            self.assertEqual(len(role["abilityIds"]), len(set(role["abilityIds"])))
            self.assertGreaterEqual(role["emergencyRefillCharges"], 1)

        archetypes = self.documents["GameplayEnemyArchetypes.json"]["archetypes"]
        self.assertEqual(len({item["id"] for item in archetypes}), len(archetypes))

        encounters = self.documents["GameplayEncounterDefinitions.json"]["encounters"]
        self.assertEqual(len({item["id"] for item in encounters}), len(encounters))
        archetype_ids = {item["id"] for item in archetypes}
        self.assertTrue(all(item["archetypeId"] in archetype_ids for item in encounters))

        missions = self.documents["GameplayMissionDefinitions.json"]["missions"]
        self.assertEqual(len({item["id"] for item in missions}), len(missions))
        encounter_by_id = {item["id"]: item for item in encounters}
        for mission in missions:
            self.assertEqual(len(mission["cellEncounterIds"]), 2)
            self.assertEqual(len(set(mission["cellEncounterIds"])), 2)
            self.assertTrue(all(cell in encounter_by_id for cell in mission["cellEncounterIds"]))
            if mission["objectiveType"] == "Clear":
                self.assertTrue(all(encounter_by_id[cell]["spawnCount"] == 3 for cell in mission["cellEncounterIds"]))

    def test_layout_is_parseable_but_runtime_blocked(self) -> None:
        layout_document = self.documents["GameplayArenaLayouts.json"]
        self.assertEqual(layout_document["surveyStatus"], "BLOCKED")
        self.assertEqual(layout_document["blocker"], "ANCHOR_SURVEY_UNVERIFIED")
        self.assertTrue(all(not layout["verified"] and not layout["requiredAnchors"] for layout in layout_document["layouts"]))

    def test_public_snapshot_does_not_replicate_authority_ledger(self) -> None:
        state_header = (ROOT / "Source/Aura/Public/Gameplay/AuraMissionState.h").read_text(encoding="utf-8")
        self.assertIn("FAuraMissionPublicSnapshot ReplicatedPublicSnapshot", state_header)
        self.assertNotIn("FAuraMissionRunState ReplicatedRunState", state_header)


if __name__ == "__main__":
    unittest.main(verbosity=2)
