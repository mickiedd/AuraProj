#!/usr/bin/env python3
"""Static checks for the inert Day 51 cooperative-recovery contract."""
from __future__ import annotations

import json
import re
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Content/Config/GameplayCombatTuning.json"
HEADER = ROOT / "Source/Aura/Public/Gameplay/AuraRunRecoveryTypes.h"
SOURCE = ROOT / "Source/Aura/Private/Gameplay/AuraRunRecoveryTypes.cpp"
NATIVE = ROOT / "Source/Aura/Private/Tests/AuraGameplayDay51Tests.cpp"
GAME_MODE = ROOT / "Source/Aura/Private/Game/AuraGameModeBase.cpp"
CHARACTER = ROOT / "Source/Aura/Private/Character/AuraCharacter.cpp"
COMBAT_TYPES = ROOT / "Source/Aura/Public/Combat/AuraCombatTypes.h"


class GameplayDay51ContractTests(unittest.TestCase):
    def test_recovery_values_are_closed_and_fail_closed(self) -> None:
        recovery = json.loads(CONFIG.read_text(encoding="utf-8"))["cooperativeRecovery"]
        self.assertEqual(recovery["status"], "CONTRACT_ONLY")
        self.assertEqual(recovery["runtimeWiring"], "UNWIRED")
        self.assertEqual(recovery["channelSeconds"], 3.0)
        self.assertEqual(recovery["attachmentRetryDelaySeconds"], 1.0)
        self.assertEqual(recovery["attachmentRetryCount"], 1)
        self.assertEqual(recovery["disconnectLeaseSeconds"], 60.0)
        self.assertEqual(recovery["soloCharges"], 1)
        self.assertEqual(recovery["pairSharedCharges"], 2)
        self.assertEqual(recovery["recoveredHealthFraction"], 0.35)
        self.assertEqual(recovery["recoveredManaFraction"], 0.50)
        self.assertTrue(recovery["legacyRecoveryUnchanged"])
        self.assertFalse(recovery["globalDownedLifeEnumAdded"])
        self.assertIn("ANCHOR_SURVEY_UNVERIFIED", recovery["runtimeGates"])

    def test_reducer_is_inert_and_legacy_paths_are_untouched(self) -> None:
        header = HEADER.read_text(encoding="utf-8")
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("Inert Day 51 authority reducer", header)
        self.assertIn("TechnicalRecoveryFailure", source)
        self.assertNotIn("AuraRunRecoveryTypes", GAME_MODE.read_text(encoding="utf-8"))
        self.assertNotIn("AuraRunRecoveryTypes", CHARACTER.read_text(encoding="utf-8"))
        combat = COMBAT_TYPES.read_text(encoding="utf-8")
        self.assertNotIn("AwaitingRescue", combat)
        self.assertIn("EAuraCombatLifeState", combat)
        game_mode = GAME_MODE.read_text(encoding="utf-8")
        self.assertIn("bEnablePlayerRespawn", game_mode)
        self.assertIn("RestartPlayer(DeadController)", game_mode)

    def test_all_named_native_cases_are_present(self) -> None:
        expected = {
            "DeathDispatchOnce", "RescueRaceSingleCharge", "RecoverRetainsRunState",
            "ReconnectDoesNotResurrect", "RecoveryAttachmentBeforeAlive",
            "DisconnectRetainsCombatAndCooldowns", "ProxyWipeAndExpiry",
            "TeamWipeFails", "LegacyRecoveryUnchanged",
        }
        native = NATIVE.read_text(encoding="utf-8")
        self.assertEqual(set(re.findall(r'"Aura\.Gameplay\.Day51\.([A-Za-z0-9]+)"', native)), expected)


if __name__ == "__main__":
    unittest.main(verbosity=2)
