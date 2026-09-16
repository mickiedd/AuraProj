import unreal
print('TAKE_HR', unreal.AutomationLibrary.take_high_res_screenshot.__doc__)
print('CAM_INFO', unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_level_viewport_camera_info())
