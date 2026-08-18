#!/usr/bin/env python3
"""
Structural test for Content/AbilityDefinitions/FireBolt.xml.

Validates the fix for the LMB ability hang regression introduced by commit c6eff50
("Add 1s wait before montage event"). That commit inserted a <node class="Wait">
between PlayMontage and WaitForMontageEvent. Because PlayMontage returns Success
immediately (it only kicks off the montage task) and WaitForMontageEvent uses
UAbilityTask_WaitGameplayEvent (which only catches events fired AFTER it
subscribes), the 1s sleep caused WaitForMontageEvent to subscribe AFTER the
Event.Montage.FireBolt anim notify had already fired -> it hung forever, so
SpawnProjectiles never ran (no projectile) and the Sequence never completed
(ability stayed IsActive=true, blocking every later press).

This test is a static guard: it parses the real FireBolt.xml and asserts the
graph ordering is correct. It does NOT exercise the runtime (montage event
delivery), which needs the engine.

It also guards the runtime lifecycle contract: a predicted client may skip the
authority-only projectile action, but must wait for the server's authoritative
ability end instead of replicating local graph completion back to the server.
"""

import sys
import json
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
XML_PATH = REPO_ROOT / "Content" / "AbilityDefinitions" / "FireBolt.xml"
PROJECTILE_CONFIG_PATH = REPO_ROOT / "Content" / "Config" / "ProjectileDefinitions.json"
PROJECTILE_CPP_PATH = REPO_ROOT / "Source" / "Aura" / "Private" / "Actor" / "AuraProjectile.cpp"
SPAWN_PROJECTILES_CPP_PATH = (REPO_ROOT / "Plugins" / "AuraAbilityGraph" / "Source" /
                              "AuraAbilityGraph" / "Private" / "Nodes" / "Actions" /
                              "SpawnProjectilesNode.cpp")

FAILURES = []


def check(cond, msg):
    if not cond:
        FAILURES.append(msg)
    return cond


def main():
    # 1. File exists and parses.
    check(XML_PATH.is_file(), f"Missing ability XML: {XML_PATH}")
    if not XML_PATH.is_file():
        report(); return 1
    try:
        root = ET.parse(XML_PATH).getroot()
    except ET.ParseError as e:
        FAILURES.append(f"XML parse error: {e}")
        report(); return 1

    check(root.tag == "ability", f"Root tag is '{root.tag}', expected 'ability'")
    check(root.get("abilityTag") == "Abilities.Fire.FireBolt",
          f"abilityTag='{root.get('abilityTag')}', expected 'Abilities.Fire.FireBolt'")

    graph = root.find("graph")
    check(graph is not None, "No <graph> element")
    if graph is None:
        report(); return 1

    seq = graph.find("node")
    check(seq is not None and seq.get("class") == "Sequence",
          "First graph node is not a Sequence")
    if seq is None:
        report(); return 1

    children = list(seq.findall("node"))
    classes = [c.get("class") for c in children]

    # 2. No Wait node anywhere (the buggy delay node must be gone).
    check("Wait" not in classes,
          f"Found 'Wait' node(s) in Sequence (regression): classes={classes}")

    # 3. PlayMontage must be IMMEDIATELY followed by WaitForMontageEvent.
    check("PlayMontage" in classes, f"No PlayMontage node: {classes}")
    check("WaitForMontageEvent" in classes, f"No WaitForMontageEvent node: {classes}")
    if "PlayMontage" in classes and "WaitForMontageEvent" in classes:
        pm = classes.index("PlayMontage")
        wfme = classes.index("WaitForMontageEvent")
        check(wfme == pm + 1,
              f"WaitForMontageEvent (idx {wfme}) must immediately follow PlayMontage (idx {pm}); "
              f"classes={classes}")

    # 4. WaitForMontageEvent must use the correct event tag.
    wfme_node = next((c for c in children if c.get("class") == "WaitForMontageEvent"), None)
    if wfme_node is not None:
        et = next((p.get("value") for p in wfme_node.findall("property")
                   if p.get("name") == "EventTag"), None)
        check(et == "Event.Montage.FireBolt",
              f"WaitForMontageEvent EventTag='{et}', expected 'Event.Montage.FireBolt'")

    # 5. SpawnProjectiles must come after WaitForMontageEvent.
    check("SpawnProjectiles" in classes, f"No SpawnProjectiles node: {classes}")
    if "WaitForMontageEvent" in classes and "SpawnProjectiles" in classes:
        check(classes.index("SpawnProjectiles") > classes.index("WaitForMontageEvent"),
              f"SpawnProjectiles must come after WaitForMontageEvent; classes={classes}")

    # 6. SpawnProjectiles uses the current data-driven native projectile contract.
    sp_node = next((c for c in children if c.get("class") == "SpawnProjectiles"), None)
    if sp_node is not None:
        definition = next((p.get("value") for p in sp_node.findall("property")
                           if p.get("name") == "ProjectileDefinition"), None)
        legacy_class = next((p.get("value") for p in sp_node.findall("property")
                             if p.get("name") == "ProjectileClass"), None)
        check(definition == "fireBolt",
              f"SpawnProjectiles ProjectileDefinition='{definition}', expected 'fireBolt'")
        check(legacy_class is None,
              f"SpawnProjectiles still contains legacy ProjectileClass='{legacy_class}'")

        check(PROJECTILE_CONFIG_PATH.is_file(),
              f"Missing projectile configuration: {PROJECTILE_CONFIG_PATH}")
        if PROJECTILE_CONFIG_PATH.is_file():
            try:
                projectile_config = json.loads(PROJECTILE_CONFIG_PATH.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as e:
                FAILURES.append(f"Projectile config parse error: {e}")
            else:
                projectiles = projectile_config.get("projectiles", {})
                firebolt = projectiles.get("fireBolt") if isinstance(projectiles, dict) else None
                check(isinstance(firebolt, dict),
                      "ProjectileDefinitions.json has no 'fireBolt' definition")
                if isinstance(firebolt, dict):
                    check(firebolt.get("nativeClass") == "/Script/Aura.AuraProjectile",
                          "fireBolt definition must use native /Script/Aura.AuraProjectile")

    # 7. Cost + cooldown sanity (unchanged by the fix, but guards the data).
    cost = root.find("cost")
    check(cost is not None and cost.get("mana") == "10", f"cost mana='{cost.get('mana') if cost is not None else None}', expected '10'")
    cd = root.find("cooldown")
    check(cd is not None and cd.get("tag") == "Cooldown.Fire.FireBolt" and cd.get("duration") == "5",
          f"cooldown mismatch: tag='{cd.get('tag') if cd is not None else None}' dur='{cd.get('duration') if cd is not None else None}'")

    # 8. Client graph completion must wait for the authoritative server end.
    data_ability_cpp = REPO_ROOT / "Plugins" / "AuraAbilityGraph" / "Source" / "AuraAbilityGraph" / "Private" / "DataAbility.cpp"
    check(data_ability_cpp.is_file(), f"Missing data ability implementation: {data_ability_cpp}")
    if data_ability_cpp.is_file():
        source = data_ability_cpp.read_text(encoding="utf-8")
        check("Graph completed on non-authority client; waiting for authoritative server end" in source,
              "DataAbility must hold client graph completion until the authoritative server ends the activation")
        check("ShouldEndAfterGraphCompletion" in source,
              "DataAbility must use the graph-completion authority boundary contract")

    # 9. Client replicas must ignore the source avatar before spawning impact FX. DamageEffectParams
    # is server-only, so the collision guard must use replicated Owner/Instigator identity.
    check(PROJECTILE_CPP_PATH.is_file(), f"Missing projectile implementation: {PROJECTILE_CPP_PATH}")
    if PROJECTILE_CPP_PATH.is_file():
        projectile_source = PROJECTILE_CPP_PATH.read_text(encoding="utf-8")
        check("GetOwner() == OtherActor" in projectile_source,
              "Projectile overlap guard must reject the replicated source Owner")
        check("GetInstigator() == OtherActor" in projectile_source,
              "Projectile overlap guard must reject the replicated source Instigator")

    check(SPAWN_PROJECTILES_CPP_PATH.is_file(),
          f"Missing graph projectile spawner: {SPAWN_PROJECTILES_CPP_PATH}")
    if SPAWN_PROJECTILES_CPP_PATH.is_file():
        spawn_source = SPAWN_PROJECTILES_CPP_PATH.read_text(encoding="utf-8")
        check("            Ctx.AvatarActor," in spawn_source,
              "Graph projectile Owner must be the avatar so clients can identify self-collision")
        check("            Cast<APawn>(Ctx.AvatarActor)," in spawn_source,
              "Graph projectile Instigator must be the avatar pawn when available")

    report()
    return 0 if not FAILURES else 1


def report():
    print(f"Testing: {XML_PATH}")
    if not FAILURES:
        print("  PASS: FireBolt.xml graph structure is correct "
              "(no Wait before WaitForMontageEvent; ordering intact).")
    else:
        print("  FAIL:")
        for f in FAILURES:
            print(f"    - {f}")


if __name__ == "__main__":
    sys.exit(main())
