"""Focused contract checks for dedicated-server executable selection."""

import asyncio
import ast
import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANAGER = ROOT / "Scripts" / "GameServerManager.py"


def load_manager_module():
    spec = importlib.util.spec_from_file_location("aura_game_server_manager", MANAGER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    source = MANAGER.read_text(encoding="utf-8")
    tree = ast.parse(source)
    level_config = json.loads((ROOT / "Content/Config/LevelConfig.json").read_text(encoding="utf-8"))
    startup_map = ROOT / "Content/Maps/StartupMap.umap"
    staged_server = ROOT / "Saved/StagedBuilds/WindowsServer/AuraServer.exe"

    assert startup_map.is_file(), "StartupMap source asset is missing"
    assert any(entry["mapPath"] == "/Game/Maps/StartupMap" for entry in level_config["levels"])
    assert "Saved" in source and "StagedBuilds" in source
    assert "has_cooked_payload" in source
    assert "Ignoring uncooked AuraServer binary" in source
    assert "locate_editor_exe" in source
    assert "_is_editor_client" in source
    assert "_locate_editor_exe_for_client" in source
    assert "clientEngineRoot" in source
    assert "editor_executable_name" in source
    assert "is_editor_executable" in source
    assert "persistence_provider" in source
    assert "--persistence-provider" in source
    assert "AURA_PERSISTENCE_PROVIDER" in source
    assert '"serverMode"' in source

    manager = load_manager_module()
    assert manager.editor_executable_name("UnrealEditor-Win64-DebugGame.exe") == (
        "UnrealEditor-Win64-DebugGame.exe"
    )
    assert manager.editor_executable_name("UnrealEditor-Evil.exe") == "UnrealEditor.exe"
    assert manager.is_editor_executable(Path("UnrealEditor-Win64-DebugGame.exe"))
    assert manager.is_editor_executable(Path("UnrealEditor-Cmd.exe"))
    assert not manager.is_editor_executable(Path("AuraServer.exe"))

    captured = {}

    class FakeProcess:
        pid = 12345

        @staticmethod
        def poll():
            return None

    def fake_popen(args, **kwargs):
        captured["args"] = args
        captured["kwargs"] = kwargs
        return FakeProcess()

    original_popen = manager.subprocess.Popen
    manager.subprocess.Popen = fake_popen
    try:
        entry = manager.DedicatedServerEntry(
            {
                "id": "DebugGameLaunchContract",
                "mapPath": "/Game/Maps/StartupMap",
                "port": 17777,
                "queryPort": 27017,
                "launchArgs": ["-game", "-server"],
            },
            0,
            "NULL",
        )
        assert asyncio.run(
            entry._start(Path("UnrealEditor-Win64-DebugGame.exe"))
        )
    finally:
        manager.subprocess.Popen = original_popen

    assert captured["args"][:3] == [
        "UnrealEditor-Win64-DebugGame.exe",
        str(manager.PROJECT_FILE),
        "/Game/Maps/StartupMap?port=17777",
    ]
    assert "-AuraPersistenceProvider=NULL" in captured["args"]

    with tempfile.TemporaryDirectory() as temp_dir:
        engine_root = Path(temp_dir) / "Engine"
        editor_exe = engine_root / "Binaries" / "Win64" / "UnrealEditor-Win64-DebugGame.exe"
        editor_exe.parent.mkdir(parents=True)
        editor_exe.touch()
        (engine_root / "Build").mkdir()
        (engine_root / "Build" / "Build.version").write_text("{}", encoding="utf-8")
        gsm = manager.GameServerManager("127.0.0.1", 0, "127.0.0.1", None, 0)
        resolved = gsm._locate_editor_exe_for_client(
            {
                "clientExecutable": "UnrealEditor-Win64-DebugGame.exe",
                "clientEngineRoot": str(engine_root),
            }
        )
        assert resolved == editor_exe.resolve(), resolved

    # This checkout has a staged server from the final packaged validation. If
    # it is present, the manager must be able to recognize the cooked launch
    # path rather than selecting Binaries/Win64/AuraServer.exe.
    if staged_server.is_file():
        staged_root = staged_server.parent / "Aura" / "Content"
        assert staged_root.is_dir(), "Staged server content root is missing"
        assert any((staged_root / "Paks").glob("*.pak")) or any((staged_root / "Paks").glob("*.utoc"))

    function_names = {
        node.name
        for node in ast.walk(tree)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    }
    assert {"locate_server_exe", "locate_editor_exe"}.issubset(function_names)
    print("Game Server Manager cooked-server selection contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
