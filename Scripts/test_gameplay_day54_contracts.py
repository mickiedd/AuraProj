#!/usr/bin/env python3
from pathlib import Path
import json,re,unittest,xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[1]
class D54(unittest.TestCase):
 def test_config(self):
  x=json.loads((R/'Content/Config/GameplayBossDefinitions.json').read_text());self.assertEqual(x['captain']['maximumAdds'],4);self.assertEqual(x['extraction']['terminalOrder'],['AcceptedDeaths','Wipe','Deadline','Extraction']);self.assertEqual(len(x['patterns']),3)
 def test_xml(self):
  for f in ('CaptainSweep.xml','CaptainLineCharge.xml','CaptainReinforce.xml'):self.assertEqual(ET.parse(R/'Content/AbilityDefinitions'/f).getroot().attrib['contractOnly'],'true')
 def test_named_unwired(self):
  t=(R/'Source/Aura/Private/Tests/AuraGameplayDay54Tests.cpp').read_text();self.assertEqual(len(set(re.findall(r'D54T\([^,]+,"([A-Za-z0-9]+)"\)',t))),7);self.assertNotIn('AuraBossFinaleTypes',(R/'Source/Aura/Private/Gameplay/AuraMissionSubsystem.cpp').read_text())
if __name__=='__main__':unittest.main(verbosity=2)
