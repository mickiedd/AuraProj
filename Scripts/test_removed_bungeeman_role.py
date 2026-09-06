"""Regression checks for the removal of the BungeeMan playable role."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def load(relative):
    return json.loads((ROOT / relative).read_text(encoding="utf-8"))


def test_role_registry_has_no_bungeeman():
    roles = load("Content/Config/RoleConfig.json")["roles"]
    assert "BungeeMan" not in {role["role"] for role in roles}


def test_runtime_manifests_have_no_bungeeman():
    loadouts = load("Content/Config/GameplayRoleLoadouts.json")
    assert all(role["id"] != "BungeeMan" for role in loadouts["roles"])

    manifest = load("Content/Config/PlayableCandidateManifest.json")
    assert "BungeeMan" not in manifest["requiredRoles"]
    tutorial = load("Content/Config/PlayableCandidateTutorial.json")
    assert all("BungeeMan" not in step.get("roles", []) for step in tutorial["steps"])


def test_bungeeman_content_and_firegun_definition_are_removed():
    assert not any((ROOT / "Content/BungeeMan").rglob("*"))
    assert not (ROOT / "Content/AbilityDefinitions/FireGun.xml").exists()
