"""Render original generated mesh with VTK software/offscreen. Requires vtk, PIL, numpy."""
import sys
from pathlib import Path
import numpy as np
import vtk
from vtk.util.numpy_support import numpy_to_vtk,numpy_to_vtkIdTypeArray
from PIL import Image,ImageDraw,ImageFont
sys.path.insert(0,str(Path(__file__).parent))
from build_gate import build,ROOT,BASECOL
b=build(1)
r=vtk.vtkRenderer();r.SetBackground(.78,.84,.89);r.SetBackground2(.94,.95,.96);r.GradientBackgroundOn()
for name,arrays in b.a.items():
    if not arrays['f']:continue
    v=np.array(arrays['v'],np.float32);faces=np.array(arrays['f'],np.int64)
    poly=vtk.vtkPolyData();pts=vtk.vtkPoints();pts.SetData(numpy_to_vtk(v,deep=True));poly.SetPoints(pts)
    packed=np.column_stack([np.full(len(faces),3,dtype=np.int64),faces]).reshape(-1)
    cells=vtk.vtkCellArray();cells.SetCells(len(faces),numpy_to_vtkIdTypeArray(packed,deep=True));poly.SetPolys(cells)
    normals=vtk.vtkPolyDataNormals();normals.SetInputData(poly);normals.ConsistencyOn();normals.AutoOrientNormalsOff();normals.SplittingOff();normals.Update()
    mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(normals.GetOutputPort())
    actor=vtk.vtkActor();actor.SetMapper(mapper)
    rgb=BASECOL[name];actor.GetProperty().SetColor(*(c/255 for c in rgb))
    if name=='Plaster':actor.GetProperty().SetColor(.57,.52,.46)
    if name=='Sign':actor.GetProperty().SetColor(.29,.15,.11)
    actor.GetProperty().SetAmbient(.24);actor.GetProperty().SetDiffuse(.83);actor.GetProperty().SetSpecular(.05)
    r.AddActor(actor)
# Stone foreground forecourt - not part of the exported gate mesh.
g=vtk.vtkCubeSource();g.SetXLength(70);g.SetYLength(53);g.SetZLength(.08);g.SetCenter(0,0,-.33)
mapper=vtk.vtkPolyDataMapper();mapper.SetInputConnection(g.GetOutputPort());a=vtk.vtkActor();a.SetMapper(mapper);a.GetProperty().SetColor(.72,.68,.60);r.AddActor(a)
light=vtk.vtkLight();light.SetPosition(-30,-36,62);light.SetFocalPoint(0,0,9);light.SetIntensity(1.15);r.AddLight(light)
light2=vtk.vtkLight();light2.SetPosition(24,18,35);light2.SetFocalPoint(0,0,6);light2.SetIntensity(.6);r.AddLight(light2)
cam=r.GetActiveCamera();cam.SetPosition(39,-54,23);cam.SetFocalPoint(0,0,9.1);cam.SetViewUp(0,0,1);cam.SetViewAngle(29)
w=vtk.vtkRenderWindow();w.SetOffScreenRendering(1);w.AddRenderer(r);w.SetSize(1920,1160);w.SetMultiSamples(0)
w.Render();f=vtk.vtkWindowToImageFilter();f.SetInput(w);f.ReadFrontBufferOn();f.Update()
p=vtk.vtkPNGWriter();p.SetFileName(str(ROOT/'Preview'/'Zhengdongmen_actual_mesh_preview.png'));p.SetInputConnection(f.GetOutputPort());p.Write()
print('Rendered',ROOT/'Preview'/'Zhengdongmen_actual_mesh_preview.png',flush=True)
