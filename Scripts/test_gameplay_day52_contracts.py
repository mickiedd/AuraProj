#!/usr/bin/env python3
from pathlib import Path
import json, re, unittest
ROOT=Path(__file__).resolve().parents[1]
class Day52(unittest.TestCase):
 def test_definitions(self):
  s=json.loads((ROOT/'Content/Config/GameplaySupplyDefinitions.json').read_text()); h=json.loads((ROOT/'Content/Config/GameplayHazardDefinitions.json').read_text())
  self.assertEqual((s['startingMedkitsPerMember'],s['startingResourcePacksPerMember'],s['emergencyCooldownSeconds']),(2,1,30)); self.assertFalse(s['persistentInventoryMutationAllowed'])
  self.assertEqual((h['hazards'][0]['cueSeconds'],h['hazards'][0]['activeSeconds'],h['hazards'][0]['cooldownSeconds']),(1,2,5)); self.assertFalse(h['optionalCache']['blocksPrimaryPath'])
 def test_inert(self):
  for path in ['Source/Aura/Private/Game/AuraGameModeBase.cpp','Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp']:
   self.assertNotIn('AuraSupplyRiskTypes',(ROOT/path).read_text())
 def test_named(self):
  expected={'SupplyAtomicAndCapped','RoleResourceApplicability','StationReceiptOnce','SupplyOwnerExtensionPreservesState','EmergencyResourceNoSoftlock','CacheOptionalAndBounded','HazardRespectsRules','HazardRegisteredSourceDamage','HazardInvalidSourceFailsClosed'}
  text=(ROOT/'Source/Aura/Private/Tests/AuraGameplayDay52Tests.cpp').read_text(); self.assertEqual(set(re.findall(r'AURA_DAY52_TEST\([^,]+, "([A-Za-z0-9]+)"\)',text)),expected)
if __name__=='__main__': unittest.main(verbosity=2)
