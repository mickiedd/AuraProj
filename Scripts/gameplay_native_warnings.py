"""Narrow warning classification for reviewed native negative test cases.

Nothing is hidden from the engine log. A message, count or test-context change
blocks acceptance. Startup, asset, null-world and attachment warnings are never
accepted by this policy. This policy does not apply to packaged gameplay.
"""
from collections import Counter
import re


POLICY_ID = "AuraRoleBattleNegativeCasesV1"
TEST_SOURCE = "Source/Aura/Private/Tests/AuraRoleBattleTests.cpp"
PREFIX = "Aura.RoleBattle."
DAMAGE = re.escape("LogAura: Warning: [DamageBoundary] Rejected damage: source or target ASC is invalid.")
POLICY = {
    PREFIX + "Day4.AuthorityRejection": {
        "reason": "The test submits missing source/target ASCs and asserts both rejections.",
        "patterns": [(DAMAGE, 2)],
    },
    PREFIX + "Day4.DirectCauseDamageBoundary": {
        "reason": "The test calls CauseDamage with a null target to verify safe rejection.",
        "patterns": [(DAMAGE, 1)],
    },
    PREFIX + "Day4.NeutralDamageCoefficients": {
        "reason": "An intentionally empty curve table must use neutral coefficients for all three rows.",
        "patterns": [(re.escape("LogCurveTable: Warning: UCurveTable::FindCurve : Row '" + row +
                                "' not found in CurveTable '/Engine/Transient.CurveTable_") + r"[0-9]+" + re.escape("' ()."), 1)
                     for row in ("ArmorPenetration", "EffectiveArmor", "CriticalHitResistance")],
    },
    PREFIX + "Day5.AggregateValidationErrors": {
        "reason": "The test intentionally names four nonexistent assets and asserts aggregated validation errors.",
        "patterns": [
            (re.escape("LogStreaming: Warning: LoadPackage: SkipPackage: /Game/Missing/" + asset + " (0x") +
             r"[0-9A-Fa-f]{16}" + re.escape(") - The package to load does not exist on disk or in the loader"), 1)
            for asset in ("Mesh", "Anim_C", "Ability_C", "Definition")
        ] + [(re.escape(message), 1) for message in (
            "LogUObjectGlobals: Warning: Failed to find object 'SkeletalMesh None./Game/Missing/Mesh'",
            "LogUObjectGlobals: Warning: Failed to find object 'Class None./Game/Missing/Anim_C'",
            "LogUObjectGlobals: Warning: Failed to find object 'Class None./Game/Missing/Ability_C'",
            "LogUObjectGlobals: Warning: Failed to find object 'AuraAbilityDefinition /Game/Missing/Definition.Missing'",
        )],
    },
    PREFIX + "Day6.ClientRoleMutationRejected": {
        "reason": "A simulated-proxy PlayerState must reject the attempted role write and retain an empty role.",
        "patterns": [(re.escape("LogAura: Warning: [Role][PlayerState] Rejected non-authority mutation to 'BungeeMan' on AuraPlayerState_") + r"[0-9]+\.", 1)],
    },
    PREFIX + "Day6.RemovedRoleGrantNotPromoted": {
        "reason": "The test supplies an obsolete role grant and asserts it is quarantined rather than restored as progression.",
        "patterns": [(re.escape("LogAura: Warning: [RoleGrant] Removed/mismatched role grant 'Abilities.Lightning.Electrocute' was not restored or promoted."), 1)],
    },
}


def classify(log_text, tests):
    known_tests = {test["fullTestPath"]: test for test in tests}
    active, started, finished = None, set(), set()
    observed, matched = Counter(), Counter()
    issues, accepted = [], []
    for line_number, line in enumerate(log_text.splitlines(), 1):
        start = re.search(r"LogAutomationController: Display: Test Started\. Name=\{[^}]*\} Path=\{([^}]+)\}", line)
        end = re.search(r"LogAutomationController: Display: Test Completed\. Result=\{[^}]+\} Name=\{[^}]*\} Path=\{([^}]+)\}", line)
        if start:
            name = start.group(1)
            if active is not None or name not in known_tests or name in started:
                issues.append({"lineNumber": line_number, "reason": "INVALID_TEST_START"})
            active = name
            started.add(name)
        warning = re.search(r"\b(Log[A-Za-z0-9_]+:\s*Warning:.*)$", line)
        if warning:
            if active is None or active not in POLICY:
                issues.append({"lineNumber": line_number, "reason": "WARNING_OUTSIDE_REVIEWED_TEST", "test": active})
            else:
                observed[active] += 1
                matches = [i for i, (pattern, _) in enumerate(POLICY[active]["patterns"]) if re.fullmatch(pattern, warning.group(1))]
                if len(matches) != 1:
                    issues.append({"lineNumber": line_number, "reason": "UNREVIEWED_WARNING_MESSAGE", "test": active})
                else:
                    matched[(active, matches[0])] += 1
                    accepted.append({"lineNumber": line_number, "test": active, "case": matches[0]})
        if end:
            name = end.group(1)
            if active != name or name in finished:
                issues.append({"lineNumber": line_number, "reason": "INVALID_TEST_END"})
            finished.add(name)
            active = None
    if active is not None or started != set(known_tests) or finished != set(known_tests):
        issues.append({"reason": "TEST_LIFECYCLE_INCOMPLETE"})
    for name, test in known_tests.items():
        if test["warnings"] != observed[name]:
            issues.append({"reason": "WARNING_COUNT_EXPORT_MISMATCH", "test": name})
        if name in POLICY:
            for i, (_, expected) in enumerate(POLICY[name]["patterns"]):
                if matched[(name, i)] != expected:
                    issues.append({"reason": "REVIEWED_WARNING_COUNT_CHANGED", "test": name, "case": i})
    return {"policyId": POLICY_ID, "source": TEST_SOURCE, "status": "BLOCKED" if issues else "PASS",
            "classifiedCount": len(accepted), "classified": accepted, "issues": issues,
            "rationales": {name: POLICY[name]["reason"] for name in known_tests if name in POLICY}}
