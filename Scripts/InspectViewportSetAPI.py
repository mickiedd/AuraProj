import unreal
for cls in [unreal.UnrealEditorSubsystem, unreal.LevelEditorSubsystem, unreal.EditorLevelLibrary]:
    print('CLS', cls.__name__)
    for name in dir(cls):
        if 'viewport_camera' in name or 'ortho' in name or 'game_view' in name:
            try:
                print(name, getattr(cls, name).__doc__)
            except Exception as e:
                print(name, e)
