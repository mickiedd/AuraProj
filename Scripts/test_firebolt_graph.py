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
"""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
XML_PATH = REPO_ROOT / "Content" / "AbilityDefinitions" / "FireBolt.xml"

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

    # 6. SpawnProjectiles projectile class is the FireBolt BP.
    sp_node = next((c for c in children if c.get("class") == "SpawnProjectiles"), None)
    if sp_node is not None:
        pc = next((p.get("value") for p in sp_node.findall("property")
                   if p.get("name") == "ProjectileClass"), None)
        check(pc and "BP_FireBolt" in pc,
              f"SpawnProjectiles ProjectileClass='{pc}', expected to reference BP_FireBolt")

    # 7. Cost + cooldown sanity (unchanged by the fix, but guards the data).
    cost = root.find("cost")
    check(cost is not None and cost.get("mana") == "10", f"cost mana='{cost.get('mana') if cost is not None else None}', expected '10'")
    cd = root.find("cooldown")
    check(cd is not None and cd.get("tag") == "Cooldown.Fire.FireBolt" and cd.get("duration") == "5",
          f"cooldown mismatch: tag='{cd.get('tag') if cd is not None else None}' dur='{cd.get('duration') if cd is not None else None}'")

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