"""Focused contracts for the Civilian Unreal Behavior Tree and ground placement."""

import argparse
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def check_runtime_log(log_path: Path, failures: list[str]) -> None:
    if not log_path.is_file():
        failures.append(f"Runtime log does not exist: {log_path}")
        return

    log = log_path.read_text(encoding="utf-8", errors="replace")
    require("[CivilianAI] BT started" in log, "runtime: Civilian BT never started", failures)
    destination_count = log.count("[CivilianAI][BT]") - log.count("[CivilianAI][BT] No reachable destination")
    require(destination_count >= 3, "runtime: BT did not select destinations for all Civilians", failures)
    require("[Civilian][Ground]" in log, "runtime: no Civilian ground-placement marker", failures)
    require("Failed to start BT" not in log, "runtime: BT startup failure present", failures)

    ground_rows = re.findall(r"\[Civilian\]\[Ground\].*?floorZ=([-+0-9.]+).*?actorZ=([-+0-9.]+)", log)
    require(bool(ground_rows), "runtime: ground marker has no floor/actor Z values", failures)
    for floor_z, actor_z in ground_rows:
        require(abs(float(actor_z) - float(floor_z)) > 1.0,
                "runtime: actor Z was not offset above the floor by capsule height", failures)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", type=Path, help="also validate a captured dedicated-server/editor-server log")
    args = parser.parse_args()

    failures: list[str] = []
    civilian_cpp = read("Source/Aura/Private/Character/AuraCivilian.cpp")
    civilian_h = read("Source/Aura/Public/Character/AuraCivilian.h")
    controller_cpp = read("Source/Aura/Private/AI/AuraCivilianAIController.cpp")
    controller_h = read("Source/Aura/Public/AI/AuraCivilianAIController.h")
    destination_cpp = read("Source/Aura/Private/AI/BTTask_FindCivilianDestination.cpp")
    move_cpp = read("Source/Aura/Private/AI/BTTask_CivilianMoveTo.cpp")
    spawn_cpp = read("Source/Aura/Private/World/AuraCivilianSpawnVolume.cpp")
    manager_cpp = read("Source/Aura/Private/World/AuraPopulationManager.cpp")

    require("AIControllerClass = AAuraCivilianAIController::StaticClass()" in civilian_cpp,
            "Civilian does not select the native AI controller", failures)
    require("AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned" in civilian_cpp,
            "Civilian AI is not auto-possessed for runtime and spawned actors", failures)
    require("SpawnDefaultController()" in civilian_cpp, "Civilian does not repair a missing server controller", failures)
    require("StartCivilianBehavior()" in civilian_cpp, "Civilian does not start its BT after role initialization", failures)
    require("AIControllerClass = nullptr" not in civilian_cpp and "EAutoPossessAI::Disabled" not in civilian_cpp,
            "Civilian still contains the old disabled-AI configuration", failures)

    require("LineTraceSingleByObjectType" in civilian_cpp, "Civilian has no final floor trace", failures)
    require("GetScaledCapsuleHalfHeight" in civilian_cpp, "ground correction does not use the scaled capsule", failures)
    require("GroundZ + CapsuleHalfHeight + 2.f" in civilian_cpp,
            "ground correction does not place the capsule center above the floor", failures)
    require("SetMovementMode(MOVE_Walking)" in civilian_cpp, "ground correction does not restore walking mode", failures)
    require("EAuraCivilianActivity" in civilian_h, "Civilian activity state is not declared", failures)

    require("UBehaviorTree" in controller_h and "UBlackboardData" in controller_h,
            "Civilian controller does not own standard Unreal BT/BB assets", failures)
    for key in ("ThreatActor", "ThreatDistance", "Destination", "Activity", "HomeLocation",
                "WorkMarker", "ObservationMarker", "ShelterMarker", "LastMoveFailureTime"):
        require(f'"{key}"' in controller_cpp, f"blackboard contract missing key {key}", failures)
    require("RunBehaviorTree(CivilianBehaviorTree)" in controller_cpp,
            "Civilian controller does not run a standard Unreal Behavior Tree", failures)
    require("StopLogic" in controller_cpp and "StopMovement" in controller_cpp,
            "Civilian controller does not stop AI on life-state exit", failures)

    require("GetRandomReachablePointInRadius" in destination_cpp,
            "Civilian BT destination task does not use navigation reachability", failures)
    require("ProjectPointToNavigation" in destination_cpp,
            "Civilian BT does not project the capsule center onto navigation data", failures)
    require("SetCivilianActivity(EAuraCivilianActivity::Wander)" in destination_cpp,
            "Civilian BT does not publish Wander activity", failures)
    require("BlackboardKey.SelectedKeyName = TEXT(\"Destination\")" in move_cpp,
            "Civilian MoveTo task is not bound to Destination", failures)
    require("bRequireNavigableEndLocation" in move_cpp and "bProjectGoalLocation" in move_cpp,
            "Civilian MoveTo task lacks navigable-end safeguards", failures)

    require("ProjectedLocation.Location + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f)" in spawn_cpp,
            "spawn volume still uses nav-surface Z as the capsule center", failures)
    require("GetScaledCapsuleHalfHeight" in spawn_cpp and "MakeCapsule" in spawn_cpp,
            "spawn overlap test does not use the Civilian capsule dimensions", failures)
    require("Civilian->AlignToGroundForSpawn()" in manager_cpp,
            "population manager does not apply final ground correction", failures)

    if args.log:
        check_runtime_log(args.log, failures)

    if failures:
        print("Civilian AI/ground contracts: FAIL")
        for failure in failures:
            print(f"- {failure}")
        return 1

    runtime_suffix = " plus runtime log" if args.log else ""
    print(f"Civilian AI/ground contracts: PASS (focused checks{runtime_suffix})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
