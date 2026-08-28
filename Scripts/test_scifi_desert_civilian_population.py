"""Regression tests for the Scifi Desert AI-Civilian level integration.

These checks cover the checked-in population data and editor topology script.
Pass the captured commandlet/server logs to validate the saved level and the
runtime population/Behavior Tree behavior as well:

    python Scripts/test_scifi_desert_civilian_population.py \
        --topology-log Saved/Logs/ConfigureDesertCivilian-2.log \
        --runtime-log Saved/Logs/ScifiDesertCivilianServer-2.log
"""

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_VOLUME_IDS = [f"DesertVillageCiviliansVolume_{index:02d}" for index in range(32)]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def check_population_config(failures: list[str]) -> None:
    level_asset = ROOT / "Content/Scifi_desert_city/Level/L_showcase_level.umap"
    require(level_asset.is_file() and level_asset.stat().st_size > 0,
            "saved Scifi Desert showcase level asset is missing or empty", failures)
    try:
        config = json.loads(read("Content/Config/PopulationSpawnTable.json"))
    except (OSError, json.JSONDecodeError) as exc:
        failures.append(f"population config cannot be parsed: {exc}")
        return

    rows = [row for row in config.get("populations", []) if row.get("populationId") == "DesertVillageCivilians"]
    require(len(rows) == 1, "population config must contain exactly one DesertVillageCivilians row", failures)
    if len(rows) != 1:
        return

    row = rows[0]
    require(row.get("mapId") == "Scifi_Desert_Level", "population row mapId is incorrect", failures)
    require(row.get("zoneId") == "DesertVillages", "population row zoneId is incorrect", failures)
    require(row.get("roleId") == "Civilian", "population row roleId is incorrect", failures)
    require(row.get("actorClass") == "/Script/Aura.AuraCivilian", "population actor class is incorrect", failures)
    require(row.get("spawnVolumeIds") == EXPECTED_VOLUME_IDS,
            "population row must contain the ordered 00-31 village volume ids", failures)
    require(row.get("initialCount") == 32 and row.get("maximumCount") == 32,
            "population row must keep 32 initial and maximum members", failures)
    require(row.get("defaultWorkProfileId") == "Observer", "population work profile is incorrect", failures)
    require(row.get("memberOverrides") == [], "desert civilians must not inherit market overrides", failures)
    require(row.get("spawnOnLoad") is True, "desert civilians must spawn on load", failures)

    try:
        battle_config = json.loads(read("Content/Config/BattleZones.json"))
    except (OSError, json.JSONDecodeError) as exc:
        failures.append(f"battle-zone config cannot be parsed: {exc}")
        return

    zones = [
        zone for zone in battle_config.get("zones", [])
        if zone.get("id") == row.get("zoneId") and zone.get("mapId") == row.get("mapId")
    ]
    require(len(zones) == 1,
            "DesertVillageCivilians must reference exactly one registered zone on Scifi_Desert_Level", failures)
    if len(zones) == 1:
        zone = zones[0]
        policy = zone.get("policy", {})
        extent = zone.get("extent", {})
        require(all(extent.get(axis, 0) > 0 for axis in ("x", "y", "z")),
                "DesertVillages must have positive map-wide bounds", failures)
        require(policy == {
            "allowPvP": False,
            "allowPlayerToCivilian": False,
            "allowEnemyToCivilian": False,
            "targetProtected": True,
        }, "DesertVillages must remain fail-closed", failures)


def check_topology_script(failures: list[str]) -> None:
    script = read("Scripts/configure_desert_civilian_level.py")
    for marker in (
        'LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"',
        'ZONE_ID = "DesertVillages"',
        "EXPECTED_VILLAGES = 32",
        "central_index = max(range(len(clusters)), key=lambda index: len(clusters[index]))",
        "villages = [cluster for index, cluster in enumerate(clusters) if index != central_index]",
        "set_property(volume, \"candidate_extents\", unreal.Vector(4000.0, 4000.0, 750.0))",
        "set_property(shelter, \"protection_radius\", 600.0)",
        "unreal.EditorLevelLibrary.save_current_level()",
    ):
        require(marker in script, f"topology script is missing contract: {marker}", failures)


def check_native_distribution_and_bounds(failures: list[str]) -> None:
    manager = read("Source/Aura/Private/World/AuraPopulationManager.cpp")
    volume = read("Source/Aura/Private/World/AuraCivilianSpawnVolume.cpp")
    hostile_service = read("Source/Aura/Private/AI/BTService_FindNearestHostile.cpp")
    require("FindVolumeForRow(Row, SlotIndex, Attempt)" in manager,
            "population manager is not using slot-aware volume selection", failures)
    require("const int32 Index = (SlotIndex + AttemptIndex + Offset) % Row.SpawnVolumeIds.Num();" in manager,
            "population manager volume distribution formula is missing", failures)
    require("void AAuraCivilianSpawnVolume::OnConstruction" in volume,
            "spawn volume does not synchronize serialized bounds during construction", failures)
    require("ApplyCandidateExtents();" in volume,
            "spawn volume does not apply candidate bounds during runtime", failures)
    require("Volume->SetBoxExtent(CandidateExtents);" in volume,
            "spawn volume does not copy CandidateExtents to its collision component", failures)
    require("RuntimeSlot->State == EAuraPopulationSlotState::Empty" in manager
            and "emptySavedSlots" in manager,
            "population manager must reconcile persisted Empty slots into initial members", failures)
    require("OwnerComp.GetBlackboardComponent()" in hostile_service,
            "native hostile service does not use its owning Behavior Tree component", failures)
    require("UBTFunctionLibrary" not in hostile_service,
            "native hostile service must not use Blueprint-only blackboard helpers", failures)
    require("StripIndexedPrefix(TEXT(\"UEDPIE_\"))" in manager and "GetAssetName" in manager,
            "population manager does not resolve configured maps from PIE-generated level names", failures)


def check_topology_log(log_path: Path, failures: list[str]) -> None:
    if not log_path.is_file():
        failures.append(f"topology log does not exist: {log_path}")
        return
    log = log_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"\[DesertCivilianTopology\] removed=(\d+) villages=(\d+) volumes=(\d+) markers=(\d+) saved=(\w+) centralClusterBuildings=(\d+)",
        log,
    )
    require(match is not None, "topology log has no saved topology summary", failures)
    if match:
        removed, villages, volumes, markers, saved, central_buildings = match.groups()
        require(villages == "32", f"topology log villages={villages}, expected 32", failures)
        require(volumes == "32", f"topology log volumes={volumes}, expected 32", failures)
        require(markers == "48", f"topology log markers={markers}, expected 48", failures)
        require(saved.lower() == "true", "topology commandlet did not save the level", failures)
        require(int(central_buildings) > 32, "topology log did not identify the central city cluster", failures)


def check_runtime_log(log_path: Path, failures: list[str]) -> None:
    if not log_path.is_file():
        failures.append(f"runtime log does not exist: {log_path}")
        return
    log = log_path.read_text(encoding="utf-8", errors="replace")
    checks = {
        "volume registrations": (r"\[Population\]\[Volume\] Registered id=DesertVillageCiviliansVolume_", 32),
        "marker registrations": (r"\[CivilianAI\]\[Marker\] Registered id=DesertVillage", 48),
        "population spawns": (r"\[Population\]\[Spawn\] population=DesertVillageCivilians", 32),
        "BT starts": (r"\[CivilianAI\] BT started", 32),
    }
    for label, (pattern, expected) in checks.items():
        actual = len(re.findall(pattern, log))
        require(actual == expected, f"runtime {label}={actual}, expected {expected}", failures)
    require("[Population][Finalize] generation=1 map=Scifi_Desert_Level liveMembers=32 success=1" in log,
            "runtime population finalization did not report 32 live members", failures)
    require("[Population][Volume] Candidate rejected" not in log,
            "runtime rejected one or more village spawn candidates", failures)
    require("Failed to start BT" not in log, "runtime contains a Behavior Tree startup failure", failures)
    require("[WorldReadiness] State=Ready" in log, "runtime world never reached Ready", failures)
    require("[WorldReadiness] State=Unhealthy" not in log, "runtime world became unhealthy", failures)
    require("ensureAsRuntimeWarning condition failed: OwnerComp != nullptr" not in log,
            "runtime contains native Behavior Tree owner-component ensures", failures)
    require("ensureAsRuntimeWarning condition failed: BTComponent != nullptr" not in log,
            "runtime contains native Behavior Tree component ensures", failures)
    destination_count = log.count("[CivilianAI][BT]") - log.count("[CivilianAI][BT] No reachable destination")
    require(destination_count >= 3, "runtime did not exercise civilian destinations", failures)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topology-log", type=Path, help="validate the level-configuration commandlet log")
    parser.add_argument("--runtime-log", type=Path, help="validate the captured server runtime log")
    args = parser.parse_args()

    failures: list[str] = []
    check_population_config(failures)
    check_topology_script(failures)
    check_native_distribution_and_bounds(failures)
    if args.topology_log:
        check_topology_log(args.topology_log, failures)
    if args.runtime_log:
        check_runtime_log(args.runtime_log, failures)

    if failures:
        print("Scifi Desert civilian population tests: FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1

    suffix = " with topology/runtime logs" if args.topology_log and args.runtime_log else " (source/data checks)"
    print(f"Scifi Desert civilian population tests: PASS{suffix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
