#!/usr/bin/env python3
from pathlib import Path
import json,re,unittest
R=Path(__file__).resolve().parents[1]
class D56(unittest.TestCase):
 def test_guidance(self):
  x=json.loads((R/'Content/Config/GameplayGuidanceDefinitions.json').read_text());self.assertTrue(x['acceptedActionOnly']);self.assertEqual(x['ping'],{'ratePerSecond':2,'burst':3,'ttlSeconds':5,'acceptsArbitraryText':False,'requiresServerRangeLos':True});self.assertEqual(len(x['guidanceIds']),6)
 def test_named(self):
  t=(R/'Source/Aura/Private/Tests/AuraGameplayDay56Tests.cpp').read_text();self.assertEqual(len(set(re.findall(r'D56T\([^,]+,"([A-Za-z0-9]+)"\)',t))),7)
 def test_unwired(self):
  self.assertNotIn('AuraMissionPresentationTypes',(R/'Source/Aura/Private/UI/HUD/AuraHUD.cpp').read_text())
if __name__=='__main__':unittest.main(verbosity=2)
