import unreal
print("EDITOR_SUBSYSTEM", [n for n in dir(unreal.UnrealEditorSubsystem) if "camera" in n.lower() or "viewport" in n.lower() or "pilot" in n.lower()])
print("LEVEL_LIBRARY", [n for n in dir(unreal.EditorLevelLibrary) if "camera" in n.lower() or "viewport" in n.lower() or "pilot" in n.lower() or "view" in n.lower()])
print("AUTOMATION", [n for n in dir(unreal.AutomationLibrary) if "screen" in n.lower() or "view" in n.lower()])
for n in ("take_automation_screenshot_at_camera", "take_automation_screenshot", "take_high_res_screenshot"):
    fn = getattr(unreal.AutomationLibrary, n)
    print("DOC", n, getattr(fn, "__doc__", None))
print("VIEWMODE_DOC", getattr(unreal.AutomationLibrary.set_editor_viewport_view_mode, "__doc__", None))
