import json
import unreal

TARGET = '/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV4.ABP_Crunch_AuraV4'
bp = unreal.EditorAssetLibrary.load_asset(TARGET)
out = {'library': [], 'asset': bool(bp)}
try:
    lib = unreal.BlueprintEditorLibrary
    out['library'] = [n for n in dir(lib) if not n.startswith('_')]
    out['library_calls'] = {}
    for name in out['library']:
        if name in ('add_function_graph', 'add_member_variable'):
            continue
        if 'graph' not in name.lower() and 'node' not in name.lower() and 'variable' not in name.lower():
            continue
        try:
            result = getattr(lib, name)(bp)
            out['library_calls'][name] = str(result)[:4000]
        except Exception as exc:
            out['library_calls'][name] = '<error:%s>' % exc
except Exception as exc:
    out['library_error'] = str(exc)
if bp:
    out['bp_dir'] = [n for n in dir(bp) if not n.startswith('_')]
    for name in out['bp_dir']:
        if any(token in name.lower() for token in ('graph', 'variable', 'parent', 'generated')):
            try:
                out.setdefault('bp_props', {})[name] = str(bp.get_editor_property(name))[:4000]
            except Exception as exc:
                out.setdefault('bp_props', {})[name] = '<error:%s>' % exc
    try:
        out['animation_graphs'] = [str(graph) for graph in bp.get_animation_graphs()]
        for graph in bp.get_animation_graphs():
            nodes = graph.get_graph_nodes_of_class(unreal.AnimGraphNode_Base, True)
            out.setdefault('graph_nodes', {})[str(graph)] = [str(node) for node in nodes]
            out.setdefault('graph_node_props', {})[str(graph)] = []
            for node in nodes:
                item = {'node': str(node)}
                for prop in ('node', 'pins'):
                    try:
                        item[prop] = str(node.get_editor_property(prop))[:2000]
                    except Exception as exc:
                        item[prop + '_error'] = str(exc)
                out['graph_node_props'][str(graph)].append(item)
    except Exception as exc:
        out['animation_graph_error'] = str(exc)
    try:
        out['target_skeleton'] = str(bp.get_editor_property('target_skeleton'))
    except Exception as exc:
        out['target_skeleton_error'] = str(exc)
with open('C:/Git/AuraProj/Saved/Reports/CrunchMigration/blueprint-graph-probe.json', 'w', encoding='utf-8') as fh:
    json.dump(out, fh, indent=2)
unreal.log('CrunchBlueprintGraphProbeOK')
