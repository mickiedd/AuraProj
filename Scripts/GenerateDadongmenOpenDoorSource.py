"""Generate a rollback-safe Dadongmen LOD0 mesh with the baked closed leaves removed.

The original source is a single combined mesh, so the closed gate leaves cannot be
hidden by a material slot.  This script reuses the existing procedural builder,
skips only the two baked leaves and their bars, and writes a new glTF beside the
canonical source mesh.  The existing source mesh remains available for rollback.
"""
import re
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
PACKAGE = PROJECT / "Saved/RawModelImport/V4/Dadongmen_GreatEastGate_UE5"
SOURCE = PACKAGE / "Source/build_dadongmen_package.py"
OUTPUT = PACKAGE / "Meshes"
NAME = "SM_Dadongmen_OpenDoor_LOD0"


def main():
    text = SOURCE.read_text(encoding="utf-8")
    text = text.replace("ROOT = Path('/mnt/data/Dadongmen_GreatEastGate_UE5')", f"ROOT = Path(r'{PACKAGE.as_posix()}')")
    text = text.replace("REF = Path('/mnt/data/fbfc688f-6140-4f46-b80a-e49eab7f1035.png')", f"REF = Path(r'{(PACKAGE / 'Reference/Dadongmen_reference_poster.png').as_posix()}')")
    text = text.replace("shutil.copy2(REF, ROOT/'Reference'/'Dadongmen_reference_poster.png')", "None")

    # Keep the utility and geometry definitions, but avoid the source script's
    # top-level texture regeneration and preview/export block.
    texture_marker = "texture_info={}\n"
    helper_marker = "# --------------------------- geometry helpers ---------------------------"
    export_marker = "# build/export LODs"
    prefix = text[: text.index(texture_marker)]
    helpers = text[text.index(helper_marker) : text.index(export_marker)]

    # Remove the two baked closed leaves and the decorative bars from build_gate.
    helpers = re.sub(
        r"    # wooden gate leaves inside tunnel\n    if detail>=2:\n        b\.box\(\[2\.55,0\.18,4\.1\],\[-1\.34,0,2\.05\],'Wood'\);b\.box\(\[2\.55,0\.18,4\.1\],\[1\.34,0,2\.05\],'Wood'\)\n        if detail>=3:\n            for gx in \[-2\.35,-1\.55,-0\.75,0\.75,1\.55,2\.35\]:\n                for gz in np\.arange\(0\.45,3\.9,0\.65\):\n                    b\.cyl_between\(\[gx,-0\.13,gz\],\[gx,0\.13,gz\],0\.045,'Wood',sections=8\)\n",
        "    # gate leaves are authored as Blueprint detail components so they can open.\n",
        helpers,
    )

    texture_info = """texture_info={}
for nm in ['Stone','Plaster','Wood','RoofClay','Dirt','Water']:
    texture_info[nm]={
        'base':f'../Textures/{nm}/T_{nm}_BC_4K.png',
        'normal':f'../Textures/{nm}/T_{nm}_N_4K.png',
        'rough':f'../Textures/{nm}/T_{nm}_R_4K.png',
        'metal':f'../Textures/{nm}/T_{nm}_M_4K.png',
        'ao':f'../Textures/{nm}/T_{nm}_AO_4K.png',
        'height':f'../Textures/{nm}/T_{nm}_H_4K.png',
        'mr':f'../Textures/{nm}/T_{nm}_MR_4K.png'}

"""
    export_defs = text[text.index("# --------------------------- glTF writer ---------------------------") : text.index(export_marker)]
    ns = {}
    exec(prefix + texture_info + helpers + export_defs, ns)
    groups = ns["build_gate"](4)
    triangles, vertices = ns["export_gltf"](groups, NAME, OUTPUT, textured=True)
    print("DADONGMEN_OPEN_SOURCE_READY", {"path": str(OUTPUT / (NAME + ".gltf")), "triangles": triangles, "vertices": vertices})


if __name__ == "__main__":
    main()
