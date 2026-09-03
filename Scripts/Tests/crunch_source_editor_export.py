import hashlib
import json
import os
import re
from datetime import datetime, timezone

import unreal


REPO_ROOT = 'C:/Git/AuraProj'
SOURCE_ROOT = 'C:/Works/Crunch-master'
MANIFEST_PATH = os.path.join(REPO_ROOT, 'Docs/Plans/Crunch-Migration/crunch-source-asset-manifest.json')
EXPORT_ROOT = os.path.join(SOURCE_ROOT, 'Saved/CrunchMigration/Exports')
REPORT_PATH = os.path.join(REPO_ROOT, 'Saved/Reports/CrunchMigration/source-editor-export.json')


def text(value):
    try:
        return str(value)
    except Exception as exc:
        return '<unprintable:%s>' % exc


def object_path_from_manifest(path):
    rel = path.replace('Content/', '/Game/', 1).replace('.uasset', '')
    return rel + '.' + rel.rsplit('/', 1)[-1]


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest().upper()


def safe_property(obj, name):
    try:
        return {'ok': True, 'value': text(obj.get_editor_property(name))}
    except Exception as exc:
        return {'ok': False, 'error': text(exc)}


def safe_event_property(event, name):
    try:
        return text(event.get_editor_property(name))
    except Exception:
        return None


def load_manifest():
    with open(MANIFEST_PATH, 'r', encoding='utf-8') as stream:
        return json.load(stream)


def build_registry():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(['/Game/Characters/Crunch', '/Game/ParagonCrunch'], True)
    return registry


def registry_asset(registry, object_path):
    try:
        return registry.get_asset_by_object_path(object_path)
    except Exception:
        return None


def dependency_names(registry, object_path):
    try:
        options = unreal.AssetRegistryDependencyOptions()
        # AssetRegistry dependency queries are package keyed in UE5.5, while
        # the manifest uses an object path for unambiguous loading.
        package_name = object_path.rsplit('.', 1)[0]
        values = registry.get_dependencies(package_name, options)
        if values is None:
            values = []
        return sorted(set(text(value) for value in values))
    except Exception as exc:
        return {'error': text(exc)}


def source_anchor_rows(manifest, registry):
    rows = []
    for anchor in manifest['requiredAnchors']:
        object_path = object_path_from_manifest(anchor['path'])
        data = registry_asset(registry, object_path)
        source_file = os.path.join(SOURCE_ROOT, anchor['path'].replace('/', os.sep))
        actual_hash = sha256_file(source_file) if os.path.isfile(source_file) else None
        rows.append({
            'path': anchor['path'],
            'objectPath': object_path,
            'purpose': anchor['purpose'],
            'assetClass': text(data.asset_class_path) if data else None,
            'existsOnDisk': bool(actual_hash),
            'sha256': actual_hash,
            'expectedSha256': anchor['sha256'],
            'hashMatches': bool(actual_hash and actual_hash == anchor['sha256']),
            'registryFound': bool(data),
        })
    return rows


def export_dependency_closure(manifest, registry, anchors):
    rows = []
    for anchor in anchors:
        deps = dependency_names(registry, anchor['objectPath'])
        rows.append({
            'anchor': anchor['path'],
            'objectPath': anchor['objectPath'],
            'dependencies': deps,
            'dependencyReadStatus': 'READY' if isinstance(deps, list) else 'ERROR',
        })
    return {'schemaVersion': 1, 'export': 'dependencyClosure', 'rows': rows}


def blueprint_row(anchor, registry):
    path = anchor.get('objectPath') or object_path_from_manifest(anchor['path'])
    asset = unreal.EditorAssetLibrary.load_asset(path)
    row = {
        'anchor': anchor['path'],
        'objectPath': path,
        'loaded': bool(asset),
        'assetType': text(type(asset)) if asset else None,
        'generatedClass': None,
        'readableDefaults': {},
        'customDefaults': [],
        'customDefaultsStatus': 'UNAVAILABLE_PUBLIC_PYTHON_REFLECTION',
        'customDefaultsReason': 'GameplayAbilityBlueprint custom graph variables are not exposed by the public Unreal Python reflection API.',
    }
    if not asset:
        row['error'] = 'Asset failed to load'
        return row
    try:
        generated = asset.generated_class()
        row['generatedClass'] = text(generated)
        cdo = unreal.get_default_object(generated)
        for name in ('net_execution_policy', 'replication_policy', 'instancing_policy', 'ability_tags', 'activation_owned_tags', 'activation_required_tags', 'activation_blocked_tags', 'cancel_abilities_with_tag', 'block_abilities_with_tag', 'source_object', 'cooldown_gameplay_effect_class', 'cost_gameplay_effect_class'):
            result = safe_property(cdo, name)
            if result['ok']:
                row['readableDefaults'][name] = result['value']
    except Exception as exc:
        row['error'] = text(exc)
    return row


def export_blueprint_defaults(manifest, registry, anchors):
    rows = []
    for anchor in anchors:
        if 'defaults' in anchor['purpose'] or 'blueprint' in anchor['purpose']:
            rows.append(blueprint_row(anchor, registry))
    return {'schemaVersion': 1, 'export': 'blueprintDefaults', 'rows': rows}


def montage_row(anchor):
    path = anchor.get('objectPath') or object_path_from_manifest(anchor['path'])
    montage = unreal.EditorAssetLibrary.load_asset(path)
    row = {
        'anchor': anchor['path'],
        'objectPath': path,
        'loaded': bool(montage),
        'playLength': None,
        'sequenceLength': None,
        'slotNames': [],
        'sections': [],
        'sectionTimingStatus': 'UNAVAILABLE_PUBLIC_PYTHON_REFLECTION',
        'sectionTimingReason': 'AnimMontage CompositeSections is protected; public Python exposes names/count but not authored start times.',
    }
    if not montage:
        row['error'] = 'Asset failed to load'
        return row
    for method, key in (('get_play_length', 'playLength'), ('get_num_sections', 'sectionCount')):
        try:
            row[key] = getattr(montage, method)()
        except Exception as exc:
            row[key + 'Error'] = text(exc)
    try:
        row['sequenceLength'] = montage.get_editor_property('sequence_length')
    except Exception:
        pass
    try:
        row['slotNames'] = [text(value) for value in unreal.AnimationLibrary.get_montage_slot_names(montage)]
    except Exception as exc:
        row['slotNamesError'] = text(exc)
    try:
        for index in range(int(row.get('sectionCount') or 0)):
            row['sections'].append({'index': index, 'name': text(montage.get_section_name(index))})
    except Exception as exc:
        row['sectionsError'] = text(exc)
    return row


def montage_anchors(manifest):
    return [a for a in manifest['requiredAnchors'] if 'montage' in a['purpose']]


def export_montage_sections(manifest):
    return {'schemaVersion': 1, 'export': 'montageSectionTimes', 'rows': [montage_row(a) for a in montage_anchors(manifest)]}


def notify_row(event, index, track=None):
    row = {
        'index': index,
        'track': track,
        'notifyName': safe_event_property(event, 'notify_name'),
        'notifyObject': safe_event_property(event, 'notify'),
        'notifyStateClass': safe_event_property(event, 'notify_state_class'),
        'montageTickType': safe_event_property(event, 'montage_tick_type'),
        'triggerOnDedicatedServer': safe_event_property(event, 'trigger_on_dedicated_server'),
        'triggerOnFollower': safe_event_property(event, 'trigger_on_follower'),
        'triggerChance': safe_event_property(event, 'notify_trigger_chance'),
    }
    try:
        row['triggerTime'] = unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event)
        row['duration'] = unreal.AnimationLibrary.get_anim_notify_event_duration(event)
    except Exception as exc:
        row['timingError'] = text(exc)
    return row


def export_montage_notifies(manifest):
    rows = []
    for anchor in montage_anchors(manifest):
        object_path = anchor.get('objectPath') or object_path_from_manifest(anchor['path'])
        montage = unreal.EditorAssetLibrary.load_asset(object_path)
        row = {'anchor': anchor['path'], 'objectPath': object_path, 'loaded': bool(montage), 'tracks': [], 'notifies': []}
        if not montage:
            row['error'] = 'Asset failed to load'
            rows.append(row)
            continue
        try:
            track_names = [text(value) for value in unreal.AnimationLibrary.get_animation_notify_track_names(montage)]
            row['tracks'] = track_names
        except Exception as exc:
            row['tracksError'] = text(exc)
            track_names = []
        try:
            events = unreal.AnimationLibrary.get_animation_notify_events(montage)
            row['notifies'] = [notify_row(event, i) for i, event in enumerate(events)]
        except Exception as exc:
            row['notifiesError'] = text(exc)
        for track in track_names:
            try:
                events = unreal.AnimationLibrary.get_animation_notify_events_for_track(montage, track)
                row.setdefault('trackNotifies', {})[track] = [notify_row(event, i, track) for i, event in enumerate(events)]
            except Exception as exc:
                row.setdefault('trackNotifyErrors', {})[track] = text(exc)
        rows.append(row)
    return {'schemaVersion': 1, 'export': 'montageNotifyTimes', 'rows': rows}


def export_gameplay_cues(manifest, registry):
    rows = []
    for anchor in manifest['requiredAnchors']:
        deps = dependency_names(registry, object_path_from_manifest(anchor['path']))
        if not isinstance(deps, list):
            deps = []
        cues = [value for value in deps if 'gameplaycue' in value.lower() or '/cues/' in value.lower()]
        rows.append({'anchor': anchor['path'], 'objectPath': object_path_from_manifest(anchor['path']), 'references': sorted(set(cues))})
    return {'schemaVersion': 1, 'export': 'gameplayCueReferences', 'rows': rows, 'scanRule': 'AssetRegistry dependency paths containing GameplayCue or /Cues/.'}


def export_source_scan(manifest):
    rows = []
    source_dir = os.path.join(SOURCE_ROOT, 'Source')
    class_re = re.compile(r'\bclass(?:\s+\w+_API)?\s+(\w+)')
    refs_re = re.compile(r'\b(?:GA|GE|TA|AM|AN|U[A-Z])_[A-Za-z0-9_]+\b|\b(?:UpperCut|Dash|GroundBlast|Tornado|Crunch)\b')
    if os.path.isdir(source_dir):
        for root, _, files in os.walk(source_dir):
            for name in sorted(files):
                if not name.endswith(('.h', '.hpp', '.cpp')):
                    continue
                path = os.path.join(root, name)
                try:
                    with open(path, 'r', encoding='utf-8', errors='replace') as stream:
                        content = stream.read()
                    rows.append({
                        'path': os.path.relpath(path, SOURCE_ROOT).replace(os.sep, '/'),
                        'sha256': sha256_file(path),
                        'classes': sorted(set(class_re.findall(content))),
                        'crunchReferences': sorted(set(refs_re.findall(content))),
                    })
                except Exception as exc:
                    rows.append({'path': os.path.relpath(path, SOURCE_ROOT).replace(os.sep, '/'), 'error': text(exc)})
    return {'schemaVersion': 1, 'export': 'sourceClassReferenceScan', 'rows': rows, 'sourceRoot': SOURCE_ROOT}


def write_export(name, payload):
    os.makedirs(EXPORT_ROOT, exist_ok=True)
    path = os.path.join(EXPORT_ROOT, name + '.json')
    payload['generatedUtc'] = datetime.now(timezone.utc).isoformat()
    payload['sourceRoot'] = SOURCE_ROOT
    with open(path, 'w', encoding='utf-8') as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
    return path


manifest = load_manifest()
registry = build_registry()
anchors = source_anchor_rows(manifest, registry)
exports = {
    'dependencyClosure': export_dependency_closure(manifest, registry, anchors),
    'blueprintDefaults': export_blueprint_defaults(manifest, registry, anchors),
    'montageSectionTimes': export_montage_sections(manifest),
    'montageNotifyTimes': export_montage_notifies(manifest),
    'gameplayCueReferences': export_gameplay_cues(manifest, registry),
    'sourceClassReferenceScan': export_source_scan(manifest),
}
written = {name: write_export(name, payload) for name, payload in exports.items()}

contract = {
    'schemaVersion': 1,
    'sourceRoot': SOURCE_ROOT,
    'manifest': os.path.relpath(MANIFEST_PATH, REPO_ROOT).replace(os.sep, '/'),
    'anchorRows': anchors,
    'exports': {name: os.path.relpath(path, SOURCE_ROOT).replace(os.sep, '/') for name, path in written.items()},
    'limitations': [
        'Blueprint custom graph defaults are unavailable via public Python reflection and are recorded as explicit unavailable fields.',
        'AnimMontage CompositeSections start/end times are protected; section names/count and play lengths are exported, with timing status explicit.',
    ],
}
contract_path = os.path.join(REPO_ROOT, 'Docs/Plans/Crunch-Migration/crunch-source-ability-contract.json')
with open(contract_path, 'w', encoding='utf-8') as stream:
    json.dump(contract, stream, indent=2, sort_keys=True)

report = {
    'schemaVersion': 1,
    'status': 'READY_WITH_EXPLICIT_API_LIMITATIONS',
    'sourceRoot': SOURCE_ROOT,
    'anchorCount': len(anchors),
    'hashMatches': sum(1 for row in anchors if row['hashMatches']),
    'registryFound': sum(1 for row in anchors if row['registryFound']),
    'exports': {name: os.path.join('Saved/CrunchMigration/Exports', name + '.json') for name in exports},
    'contract': os.path.relpath(contract_path, REPO_ROOT).replace(os.sep, '/'),
}
os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
with open(REPORT_PATH, 'w', encoding='utf-8') as stream:
    json.dump(report, stream, indent=2, sort_keys=True)
unreal.log('CrunchSourceEditorExportStatus=%s' % report['status'])
unreal.log('CrunchSourceEditorExportAnchors=%d/%d' % (report['hashMatches'], report['anchorCount']))
