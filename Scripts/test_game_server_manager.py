"""Focused contract checks for dedicated-server executable selection."""

import ast
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANAGER = ROOT / "Scripts" / "GameServerManager.py"


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
    assert '"serverMode"' in source

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
