import unittest
from run_canton_runtime_check import validate
class RuntimeEvidenceTests(unittest.TestCase):
 def sample(self):return dict(editor_binary=False,pawn_traversal_performed=True,historically_accepted=False,passed=True,errors=[],width=1920,height=1080,frame_samples=10000,gpu_samples=10000,p95_ms=20,p99_ms=30,rss_peak_bytes=4*1024**3,failed_cell_samples=0,fell=False,walked_cm=19900,draw_calls_max=500,loaded_mesh_components=850,world_partition_streaming_enabled=True,streaming_level_count=4,warmup_seconds=60,capture_seconds=180,render_settings={**{'sg.'+g+'Quality':2 for g in ('ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading')},'r.ScreenPercentage':100,'r.DynamicGlobalIlluminationMethod':1,'r.ReflectionMethod':1})
 def test_valid(self):self.assertEqual(validate(self.sample(),'performance'),[])
 def test_reject_editor_missing_metrics_and_budget(self):
  for field,value in [('editor_binary',True),('gpu_samples',0),('width',1280),('p99_ms',55),('failed_cell_samples',1),('walked_cm',0),('draw_calls_max',0),('passed',False),('render_settings',{})]:
   with self.subTest(field=field):
    d=self.sample();d[field]=value;self.assertTrue(validate(d,'performance'))
 def test_reject_missing_pawn_routes(self):self.assertTrue(validate(self.sample(),'traversal'))
if __name__=='__main__':unittest.main()
