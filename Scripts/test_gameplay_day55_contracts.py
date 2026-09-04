#!/usr/bin/env python3
from pathlib import Path
import json,re,unittest
R=Path(__file__).resolve().parents[1]
class D55(unittest.TestCase):
 def test_rules(self):
  x=json.loads((R/'Content/Config/GameplayAssemblyRules.json').read_text());self.assertEqual((x['standardMatrixRows'],x['mutatorMatrixRows']),(54,12));self.assertEqual(len(x['randomStreams']),4);self.assertTrue(x['bothPlayersConfirmChanges'])
 def test_mutators(self):
  x=json.loads((R/'Content/Config/GameplayMutatorDefinitions.json').read_text());self.assertFalse(x['allowCombinedMutators']);self.assertEqual(x['rewardMultiplier'],1)
 def test_named(self):
  t=(R/'Source/Aura/Private/Tests/AuraGameplayDay55Tests.cpp').read_text();self.assertEqual(len(set(re.findall(r'D55T\([^,]+,"([A-Za-z0-9]+)"\)',t))),6)
if __name__=='__main__':unittest.main(verbosity=2)
