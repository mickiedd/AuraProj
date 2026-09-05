#!/usr/bin/env python3
"""Fail-closed Day 57 content manifest and staging validator."""
from __future__ import annotations
import argparse, hashlib, json, math, os, stat
from pathlib import Path

class ContentError(Exception): pass

EXPECTED_PATHS={
    *(f"Content/Config/{name}.json" for name in (
        "GameplayArenaLayouts","GameplayAssemblyRules","GameplayBossDefinitions","GameplayCombatTuning","GameplayEffects",
        "GameplayEncounterDefinitions","GameplayEnemyArchetypes","GameplayEscortDefinitions","GameplayGuidanceDefinitions",
        "GameplayHazardDefinitions","GameplayMasteryDefinitions","GameplayMissionDefinitions","GameplayMutatorDefinitions",
        "GameplayObjectiveDefinitions","GameplayPacingDefinitions","GameplayPerformanceBudgets","GameplayRewardDefinitions",
        "GameplayRoleLoadouts","GameplaySupplyDefinitions")),
    *(f"Content/AbilityDefinitions/{name}.xml" for name in ("CaptainLineCharge","CaptainReinforce","CaptainSweep"))}

def strict_json(path: Path):
    def pairs(items):
        out={}
        for key,value in items:
            if key in out: raise ContentError(f"{path}: duplicate key {key}")
            out[key]=value
        return out
    def constant(value): raise ContentError(f"{path}: non-finite value {value}")
    try: value=json.loads(path.read_text(encoding='utf-8-sig'),object_pairs_hook=pairs,parse_constant=constant)
    except (OSError,ValueError,UnicodeError) as exc: raise ContentError(f"{path}: {exc}") from exc
    if type(value) is not dict: raise ContentError(f"{path}: root must be object")
    return value

def safe_file(root: Path, relative: str) -> Path:
    if type(relative) is not str or not relative or relative.startswith(('/', '\\')): raise ContentError(f"unsafe path: {relative!r}")
    parts=relative.replace('\\','/').split('/')
    if any(p in ('','.','..') for p in parts): raise ContentError(f"unsafe path: {relative!r}")
    root=root.resolve(); lexical=root/Path(*parts)
    cursor=lexical
    while cursor != root:
        if cursor.exists() and stat.S_ISLNK(cursor.lstat().st_mode): raise ContentError(f"symlink component: {relative}")
        cursor=cursor.parent
    path=lexical.resolve()
    if root not in path.parents: raise ContentError(f"path escapes root: {relative}")
    if not path.is_file() or path.is_symlink(): raise ContentError(f"missing/unsafe file: {relative}")
    return path

def sha(path: Path) -> str: return hashlib.sha256(path.read_bytes()).hexdigest()

def validate(repo_root: Path, manifest_path: Path, staged_root: Path|None=None) -> dict:
    repo_root=repo_root.resolve()
    if manifest_path.is_absolute():
        try: manifest_relative=manifest_path.resolve().relative_to(repo_root).as_posix()
        except ValueError as exc: raise ContentError('manifest is outside repository root') from exc
    else: manifest_relative=manifest_path.as_posix()
    manifest=safe_file(repo_root,manifest_relative)
    data=strict_json(manifest)
    required={'schemaVersion','profile','definitionRevision','status','inventory','budget','files','legacyManifest'}
    if set(data) != required: raise ContentError(f"manifest fields mismatch: {sorted(set(data)^required)}")
    if data['schemaVersion'] != 1 or data['profile'] != 'GameplayExpansionV1' or data['status'] != 'CONTRACT_ONLY': raise ContentError('manifest identity/status mismatch')
    expected={'enemyArchetypes':4,'augments':8,'missionTemplates':3,'arenaLayouts':2,'mutators':2,'bosses':1}
    if data['inventory'] != expected: raise ContentError(f"inventory mismatch: expected {expected}")
    if data['budget'] != {'maximumAuthoredCost':100}: raise ContentError('budget mismatch')
    rows=data['files']
    if type(rows) is not list or not rows: raise ContentError('files must be nonempty array')
    seen=set(); folded=set()
    for index,row in enumerate(rows):
        if type(row) is not dict or set(row) != {'path','sha256'}: raise ContentError(f"files[{index}] fields invalid")
        rel=row['path']; expected_sha=row['sha256']
        if rel in seen or rel.casefold() in folded: raise ContentError(f"duplicate/case-colliding manifest path: {rel}")
        seen.add(rel);folded.add(rel.casefold())
        if type(expected_sha) is not str or len(expected_sha)!=64 or any(c not in '0123456789abcdef' for c in expected_sha): raise ContentError(f"invalid hash: {rel}")
        source=safe_file(repo_root,rel)
        if sha(source)!=expected_sha: raise ContentError(f"source hash mismatch: {rel}")
        if staged_root is not None and sha(safe_file(staged_root.resolve(),rel))!=expected_sha: raise ContentError(f"staged hash mismatch: {rel}")
    if seen != EXPECTED_PATHS: raise ContentError(f"manifest inventory paths mismatch: {sorted(seen^EXPECTED_PATHS)}")
    enemies=strict_json(repo_root/'Content/Config/GameplayEnemyArchetypes.json')['archetypes']
    roles=strict_json(repo_root/'Content/Config/GameplayRoleLoadouts.json')['roles']
    missions=strict_json(repo_root/'Content/Config/GameplayMissionDefinitions.json')['missions']
    layouts=strict_json(repo_root/'Content/Config/GameplayArenaLayouts.json')['layouts']
    mutators=strict_json(repo_root/'Content/Config/GameplayMutatorDefinitions.json')['mutators']
    ids=[row['id'] for row in enemies]+[ability for role in roles for ability in role['abilityIds']]+[row['id'] for row in missions]+[row['id'] for row in layouts]+[row['id'] for row in mutators]+['Captain']
    if len(ids)!=sum(expected.values()) or len({item.casefold() for item in ids})!=len(ids): raise ContentError('logical inventory IDs are missing, duplicated, or case-colliding')
    legacy=data['legacyManifest']
    if type(legacy) is not dict or set(legacy)!={'path','sha256'}: raise ContentError('legacyManifest fields invalid')
    if legacy['path'] in seen or sha(safe_file(repo_root,legacy['path']))!=legacy['sha256']: raise ContentError('legacy manifest changed or overlaps gameplay profile')
    return {'status':'PASS','validatedFiles':len(rows),'stagedCompared':staged_root is not None,'inventory':expected}

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo-root',type=Path,required=True);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--staged-root',type=Path)
    a=p.parse_args()
    try: result=validate(a.repo_root,a.manifest,a.staged_root)
    except ContentError as exc: print(json.dumps({'status':'FAIL','reason':str(exc)}));return 1
    print(json.dumps(result,sort_keys=True));return 0
if __name__=='__main__': raise SystemExit(main())
