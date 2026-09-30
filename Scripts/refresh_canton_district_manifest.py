"""Freeze both cooked Canton levels and their reproducibility evidence.

The append-only visual-change index is intentionally excluded: a later job must
extend it, so its content cannot be a stable package input.
"""
import csv
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FILES = [
    "GIS/Road_Classifications.geojson", "GIS/Drainage_Locations.geojson",
    "GIS/District_Parcel_Reference.geojson", "Maps/District_Test_Bounds.json",
    "Data/Road_Surface_Register.csv", "Data/Material_Evidence_Register.csv",
    "Data/Foliage_SpawnRules.json", "QA/Street_Grade_Profiles.csv",
    "QA/Canton_District/Build_Result.json", "QA/Canton_District/Slope_Refinement.json",
    "QA/Canton_District/Surface_Conformance.json", "QA/Canton_District/Landscape_Material.json",
    "QA/Canton_District/Wetness_Refinement.json",
    "QA/Canton_District/Reload_Validation.json", "QA/Canton_District/Capture_Settings.json",
    "QA/Canton_District/Capture_QA.json",
    "QA/Canton_District/Gate_Threshold_Attempt.json",
    "QA/Canton_District/Nav_to_Floor_Diagnostic.md",
    "QA/Canton_Days_11_20_Execution.md", "QA/Drainage_Flow_Notes.md",
    "QA/Vegetation_Exclusions.md", "QA/StoneRoad_Seams.md",
    "QA/Road_Edge_Blockout_Profile.svg",
    "QA/Road_Nav_Collision.md", "Docs/Gate_Ground_Datum.md",
    "Docs/Citywide_Terrain_Replication.md", "Data/Open_Issues.csv",
    "Review/M02_Roads_Review.md", "Review/M03_Material_QA.md",
    "Review/M04_Weathering_QA.md", "Review/M05_Final_Handoff.md",
    "Review/M05_Review_Finding_Disposition.md",
    "Review/M05_Independent_Audit_Disposition.md",
    "Review/M05_Source_to_Prototype_Change_Log.md",
    "Review/M03_Material_Atlas.png",
    "QA/Provisional/North_South_Modern_Context_Profile.csv",
    "QA/Provisional/North_South_Modern_Context_Profile.svg",
    "Review/M01_Terrain_Review.md", "Review/M05_Independent_Audit_Findings.md",
    "Data/M01_Native_Artifacts.csv", "Data/M01_Provisional_Artifacts.csv",
    "Data/Canton_Prototype_Contract.json", "Data/Source_Register.csv",
    "Data/Artifact_Manifest.csv", "Docs/Terrain_Acceptance_Budget.md",
    "Docs/Plans/Canton_1880_1900_UE5_Terrain_20_Day_Plan.md",
    "Docs/Plans/Canton-Terrain-Implementation/Day-14-m02-geometry-review-and-blocker-cleanup.md",
    "Docs/Plans/Canton-Terrain-Implementation/Resources/Day-01-Resources.md",
    "Export/Provisional/Heightmap_Metadata.json",
    "Export/Provisional/Canton_Modern_Context_SouthFirst.r16",
    "GIS/Provisional/Terrain_Confidence_PROVISIONAL.tif",
    "ContentSource/GuangzhouLandmarks/Wenmingmen/asset_manifest.json",
    "ContentSource/GuangzhouLandmarks/Wenmingmen/README_UE5.md",
    "Docs/Reports/Change-Archive/2026-09-28-canton-m05-review-hardening.svg",
    "Docs/Reports/Change-Archive/2026-09-28-canton-m05-review-hardening.md",
    "Docs/Reports/Change-Archive/2026-09-28-canton-m05-independent-audit-resolution.svg",
    "Docs/Reports/Change-Archive/2026-09-28-canton-m05-independent-audit-resolution.md",
    "Docs/Reports/Change-Archive/2026-09-27-canton-days-11-20-provisional-district.svg",
    "Docs/Reports/Change-Archive/2026-09-27-canton-days-11-20-provisional-district.md",
    "Scripts/prepare_canton_district.py", "Scripts/BuildCantonDistrictPrototype.py",
    "Scripts/RefineCantonDistrictPrototype.py", "Scripts/ConformCantonDistrictPrototype.py",
    "Scripts/RefineCantonDistrictWetness.py",
    "Scripts/AuthorCantonLandscapeMaterial.py", "Scripts/BuildCantonDistrictNavigation.py",
    "Scripts/ReviewCantonDistrictPrototype.py", "Scripts/test_canton_district_contract.py",
    "Scripts/ReviewCantonDistrictTraversal.py",
    "Scripts/run_canton_district_check.py", "Scripts/test_canton_district_check_runner.py",
    "Scripts/refresh_canton_district_freeze.py",
    "Scripts/test_canton_handoff_freeze.py",
    "Scripts/create_canton_north_south_profile.py",
    "Scripts/ProbeCantonDistrictSeams.py", "Scripts/ProbeCantonDistrictPerformance.py",
    "Scripts/create_canton_material_atlas.py", "Scripts/refresh_canton_district_manifest.py",
    "Source/AuraEditor/Public/Terrain/CantonTerrainLibrary.h",
    "Source/AuraEditor/Private/Terrain/CantonTerrainLibrary.cpp",
]
FILES.extend([
    "PlayCantonDistrict.command", "Build/Mac/AuraProj.PackageVersionCounter",
    "Source/Aura/Aura.Build.cs", "Source/AuraEditor/Private/AuraEditorModule.cpp",
    "Scripts/macos/unreal-common.sh", "Scripts/test_canton_source_manifest.py",
    "Scripts/CantonMaterialAuthoring.py", "Scripts/CompleteCantonDistrict.py",
    "Scripts/FinalizeCantonDistrict.py", "Scripts/generate_canton_district_assets.py",
    "Scripts/repair_canton_mac_framework.py", "Scripts/cook_canton_delivery.py",
    "Scripts/stage_canton_delivery.py", "Scripts/run_canton_runtime_check.py",
    "Scripts/test_canton_runtime_check.py", "Scripts/check_canton_runtime_samples.py",
    "Scripts/verify_canton_capture_pair.py", "Review/M05_2026_09_28_Handoff_Snapshot.md",
    "Docs/Reports/Change-Archive/2026-09-29-canton-district-runtime-implementation.svg",
    "Docs/Reports/Change-Archive/2026-09-29-canton-district-runtime-implementation.md",
    "Scripts/FixCantonWalledRuntime.py", "Scripts/run_canton_walled_visibility.py",
    "Docs/Reports/Change-Archive/2026-09-29-canton-walled-runtime-visibility.svg",
    "Docs/Reports/Change-Archive/2026-09-29-canton-walled-runtime-visibility.md",
    "Data/Canton_WalledCity_Provisional_Landmark_Placements.json",
    "Scripts/PlaceCantonWalledCityLandmarks.py",
    "Scripts/ValidateCantonWalledCityLandmarks.py",
    "Scripts/CaptureCantonWalledLandmarks.py",
    "Scripts/ProbeCantonWalledWallGateFit.py",
    "Scripts/plan_canton_walled_gate_wall_fit.py",
    "Scripts/FitCantonWalledCityWallsToGates.py",
    "Scripts/ValidateCantonWalledGateWallFit.py",
    "Scripts/CaptureCantonWallGateCloseups.py",
    "Scripts/SaveCantonWalledCityToDisk.py",
    "Scripts/BuildCantonWallFoundationProfile.py",
    "Scripts/ApplyCantonWallTerrainFit.py",
    "Scripts/ValidateCantonWallTerrainFit.py",
    "Scripts/DoubleCantonWallHeight.py",
    "Data/Canton_WalledCity_Provisional_Wall_Gate_Fit_Config.json",
    "Data/Canton_WalledCity_Provisional_Wall_Foundation_Profile.json",
    "Docs/Reports/Change-Archive/2026-09-30-canton-wall-terrain-fit.svg",
    "Docs/Reports/Change-Archive/2026-09-30-canton-wall-terrain-fit.md",
    "Docs/Reports/Change-Archive/2026-09-30-canton-wall-height-double.svg",
    "Docs/Reports/Change-Archive/2026-09-30-canton-wall-height-double.md",
    "Docs/Reports/Change-Archive/2026-09-30-canton-walls-fit-gates.svg",
    "Docs/Reports/Change-Archive/2026-09-30-canton-walls-fit-gates.md",
    "Docs/Reports/Change-Archive/2026-09-29-canton-walled-landmark-provisional-placement.svg",
    "Docs/Reports/Change-Archive/2026-09-29-canton-walled-landmark-provisional-placement.md",
])
for folder in ["ContentSource/CantonDistrict", "QA/Canton_Continuation",
               "Source/Aura/Public/Terrain", "Source/Aura/Private/Terrain"]:
    FILES.extend(str(p.relative_to(ROOT)) for p in (ROOT/folder).rglob("*") if p.is_file() and "__pycache__" not in p.parts)
FILES.extend(str(p.relative_to(ROOT)) for p in
             (ROOT/"Docs/Plans/Canton-Terrain-Implementation").glob("Day-*.md"))
OPTIONAL = [
    "QA/Canton_District/Navigation_Result.json",
    "QA/Canton_District/Cook_Result.json",
    "QA/Canton_District/Performance_Probe.json",
    "QA/Canton_District/Seam_Probe.json",
    "QA/Canton_District/Traversal_Route_Matrix.json",
    "QA/Canton_District/Runner_traversal.json",
    "QA/Canton_District/Runner_review.json",
    "QA/Canton_District/Runner_seam.json",
    "QA/Canton_District/dry_ground.png",
    "QA/Canton_District/after_rain_ground.png",
    "QA/Canton_District/district_overview.png",
    "QA/Canton_District/gate_approach.png",
    "QA/Canton_District/main_street.png",
    "QA/Canton_District/mixed_lane.png",
    "QA/Canton_District/courtyard_edge.png",
]
for folder in ["Content/Canton/DistrictPrototype",
               "Content/__ExternalActors__/Canton/DistrictPrototype",
               "Content/Canton/Provisional",
               "Content/__ExternalActors__/Canton/Provisional",
               "Content/__ExternalObjects__/Canton/Provisional",
               "Content/Assets/Environment/GuangzhouLandmarks/Wenmingmen"]:
    FILES.extend(str(path.relative_to(ROOT)) for path in (ROOT / folder).rglob("*")
                 if path.suffix in (".uasset", ".umap"))
for folder in ["Review/M02_Screenshots", "Review/M05_QA_Screenshots"]:
    FILES.extend(str(path.relative_to(ROOT)) for path in (ROOT / folder).glob("*.png"))
FILES.extend(str(path.relative_to(ROOT)) for path in (ROOT / "QA/UE_Import_Screenshots").glob("*")
             if path.is_file())
FILES.extend(path for path in OPTIONAL if (ROOT / path).is_file())
with (ROOT / "Data/M02_M05_District_Artifacts.csv").open("w", newline="") as stream:
    writer = csv.writer(stream, lineterminator="\n")
    writer.writerow(["path", "sha256", "bytes", "historical_status", "engine"])
    for relative in sorted(set(FILES)):
        data = (ROOT / relative).read_bytes()
        writer.writerow([relative, hashlib.sha256(data).hexdigest(), len(data),
                         "D_PROVISIONAL_ENGINEERING_ONLY", "UE 5.5.4 CL 40574608"])
print("Frozen %d district artifacts" % len(set(FILES)))
