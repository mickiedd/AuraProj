"""Create the Day 14 Interact input and prompt widget assets idempotently.

The native player-controller binding provides the F-key fallback.  This helper
deliberately avoids resaving pre-existing Blueprint/Input Mapping assets because
older binary packages in this repository are stored outside the current LFS
normalization rules.
"""

import unreal

IA_PATH = "/Game/Blueprints/Input/InputActions/IA_Interact"
PROMPT_PATH = "/Game/Blueprints/UI/Interaction/WBP_TargetPrompt"


def factory(first, second):
    cls = getattr(unreal, first, None) or getattr(unreal, second, None)
    if cls is None:
        raise RuntimeError("Missing factory {} / {}".format(first, second))
    return cls()


tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetSubsystem.get_default_object()

action = unreal.load_asset(IA_PATH)
if action is None:
    action = tools.create_asset("IA_Interact", "/Game/Blueprints/Input/InputActions", unreal.InputAction,
                                factory("InputAction_Factory", "InputActionFactory"))
    if action is None:
        raise RuntimeError("Unable to create IA_Interact")
    # Interact is a digital action, matching LMB's Boolean value type.
    lmb = unreal.load_asset("/Game/Blueprints/Input/InputActions/IA_LMB")
    if lmb is not None:
        action.set_editor_property("value_type", lmb.get_editor_property("value_type"))
    action.modify()
    assets.save_asset(action.get_path_name(), False)

prompt = unreal.load_asset(PROMPT_PATH)
if prompt is None:
    prompt = tools.create_asset("WBP_TargetPrompt", "/Game/Blueprints/UI/Interaction", unreal.WidgetBlueprint,
                                factory("WidgetBlueprintFactory", "WidgetBlueprint_Factory"))
    if prompt is None:
        raise RuntimeError("Unable to create WBP_TargetPrompt")
    prompt.modify()
    assets.save_asset(prompt.get_path_name(), False)

print("[Day14Assets] IA_Interact and WBP_TargetPrompt are present; native F fallback remains authoritative")
