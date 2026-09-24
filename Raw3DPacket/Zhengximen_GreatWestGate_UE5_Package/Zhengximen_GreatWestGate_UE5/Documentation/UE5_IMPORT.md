# UE5 Import Checklist

- Import LOD0 FBX with Combine Meshes ON.
- Disable auto-generated lightmap UVs; UV1 is already authored.
- LOD0 contains UCX proxies; confirm the gate opening remains traversable in Collision view.
- Enable Nanite on LOD0 if your project uses Nanite; retain authored LODs as fallback/platform LODs.
- Import 4K textures, with sRGB only on BaseColor.
- Normal maps are DirectX / Unreal convention.
- Build material instances from the JSON specs or run the included Python material script.
- Verify socket positions after import because FBX/UE version axis conversion can differ between pipelines.
- Run a Lumen surface-cache visualization pass and check thin eaves/dougong for card coverage.
