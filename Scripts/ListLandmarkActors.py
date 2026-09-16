"""List existing landmark-tagged actors in the showcase level."""
import unreal

LEVEL='/Game/Scifi_desert_city/Level/L_showcase_level'
def main():
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    rows=[]
    for a in actors:
        label=a.get_actor_label()
        tags=[str(t) for t in a.tags]
        if 'Guangzhou' in label or 'Landmark' in label or any('Guangzhou' in t or 'Landmark' in t for t in tags):
            o,e=a.get_actor_bounds(False)
            rows.append({'label':label,'name':a.get_name(),'class':a.get_class().get_name(),'tags':tags,
                         'location':[a.get_actor_location().x,a.get_actor_location().y,a.get_actor_location().z],
                         'bounds':[o.x-e.x,o.y-e.y,o.z-e.z,o.x+e.x,o.y+e.y,o.z+e.z]})
    unreal.log('LANDMARK_ACTOR_LIST '+str(rows))
    print('LANDMARK_ACTOR_COUNT',len(rows))

if __name__=='__main__': main()
