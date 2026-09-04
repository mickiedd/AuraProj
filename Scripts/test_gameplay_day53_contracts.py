#!/usr/bin/env python3
from pathlib import Path
import json,re,unittest
R=Path(__file__).resolve().parents[1]
class D53(unittest.TestCase):
 def test_rewards(self):
  x=json.loads((R/'Content/Config/GameplayRewardDefinitions.json').read_text()); self.assertEqual((x['primaryGold'],x['bossExtractionGold'],x['optionalCacheGold'],x['maximumPerParticipantGold']),(50,25,25,100)); self.assertEqual(x['lastHitBonusGold'],0)
 def test_allowlist(self):
  x=json.loads((R/'Content/Config/GameplayMasteryDefinitions.json').read_text()); self.assertEqual(x['durableAllowlist'],['Gold','MasteryBadges','GameplayGuidanceMask']); self.assertTrue(x['legacyTutorialBitsUnchanged'])
 def test_named_and_unwired(self):
  names=set(re.findall(r'AURA_DAY53_TEST\([^,]+,"([A-Za-z0-9]+)"\)',(R/'Source/Aura/Private/Tests/AuraGameplayDay53Tests.cpp').read_text())); self.assertEqual(len(names),10); self.assertNotIn('AuraMissionRewardSettlement',(R/'Source/Aura/Private/Game/AuraPersistenceSubsystem.cpp').read_text())
if __name__=='__main__':unittest.main(verbosity=2)
