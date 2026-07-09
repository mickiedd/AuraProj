# add_gamepad_input_assets.py
# Run inside Unreal Editor's Python (console: py add_gamepad_input_assets.py),
# OR from a normal shell via the remote-exec client:
#   python Scripts/remote_run.py Scripts/add_gamepad_input_assets.py
#
# Creates the joystick/gamepad input assets that pair with the LookAction /
# Look() code on AAuraPlayerController:
#   1. Creates Content/Blueprints/Input/InputActions/IA_Look  (Vector2D action)
#   2. Maps Gamepad_LeftStickX/Y -> IA_Move and Gamepad_RightStickX/Y -> IA_Look
#      in IMC_AuraContext
#   3. Assigns IA_Look to the LookAction property on BP_AuraPlayerController
#   4. Saves the touched assets
#
# IMPORTANT:
#   APPLY = False -> dry run: prints every step it WOULD take, changes nothing.
#   APPLY = True  -> performs the steps and saves the assets.
#
# Idempotent: re-running is safe. Existing IA_Look / mappings are skipped rather
# than overwritten, so manual tweaks you make later are preserved.
#
# Requires the editor running with Project Settings > Plugins > Python >
# "Enable Remote Execution?" = ON (only needed for the remote_run.py path).

import unreal

APPLY = True  # committed after reviewing the dry-run plan

IA_MOVE_PATH = "/Game/Blueprints/Input/InputActions/IA_Move"
IA_LOOK_PATH = "/Game/Blueprints/Input/InputActions/IA_Look"
IMC_PATH = "/Game/Blueprints/Input/IMC_AuraContext"
BP_PC_PATH = "/Game/Blueprints/Player/BP_AuraPlayerController"

# Gamepad stick keys to map. (Xbox layout; PS uses the same FKey names.)
STICK_TO_MOVE = ["Gamepad_LeftStickX", "Gamepad_LeftStickY"]
STICK_TO_LOOK = ["Gamepad_RightStickX", "Gamepad_RightStickY"]


def _log(msg):
    print("[add_gamepad_input_assets] " + msg)


def _make_key(key_name):
    """Build an FKey from a key name string. The unreal.Key struct exposes the
    underlying FName via the 'key_name' editor property (discovered by probing
    the running editor; unreal.KeyKeys / unreal.FKey are not exposed here)."""
    k = unreal.Key()
    k.set_editor_property("key_name", key_name)
    return k


def _mapping_exists(imc, action, key_name):
    for m in imc.mappings:
        try:
            if m.action == action and m.key.get_editor_property("key_name") == key_name:
                return True
        except Exception:
            continue
    return False


def _create_input_action(path, value_type):
    existing = unreal.load_asset(path)
    if existing is not None:
        if existing.value_type != value_type:
            _log("IA_Look exists with value_type={}; correcting to {}".format(
                existing.value_type, value_type))
            if APPLY:
                existing.set_editor_property("value_type", value_type)
                existing.modify()
        else:
            _log("skip create IA_Look: already exists at {} (value_type={})".format(
                path, existing.value_type))
        return existing

    package_path, asset_name = path.rsplit("/", 1)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    factory = None
    for fac_name in ("InputAction_Factory", "InputActionFactory"):
        cls = getattr(unreal, fac_name, None)
        if cls is not None:
            factory = cls()
            break

    _log("create asset {} in {} (value_type={})".format(asset_name, package_path, value_type))
    if not APPLY:
        return None

    if factory is None:
        raise RuntimeError("InputAction factory not available in this build; "
                           "create IA_Look manually then re-run for the IMC mappings.")
    # Note: this build's Python binding expects (name, package, asset_class, factory),
    # NOT the (name, package, factory, asset_class) order some UE docs show.
    action = asset_tools.create_asset(asset_name, package_path, unreal.InputAction, factory)
    if action is None:
        raise RuntimeError("create_asset returned None for {}".format(path))
    # value_type is read-only via direct attribute assignment; use the editor
    # property setter (the property is EditAnywhere on UInputAction).
    action.set_editor_property("value_type", value_type)
    action.modify()
    return action


def _add_mapping(imc, action, key_name):
    if _mapping_exists(imc, action, key_name):
        _log("skip mapping {} -> {} (already present)".format(key_name, action.get_name()))
        return False

    _log("map {} -> {}".format(key_name, action.get_name()))
    if not APPLY:
        return True

    key = _make_key(key_name)
    imc.map_key(action, key)
    return True


def _set_bp_look_action(blueprint_path, look_action):
    """Assign IA_Look to the LookAction EditAnywhere property on a Blueprint's
    class defaults. Best-effort: persistence of CDO edits back to the Blueprint
    asset varies by UE version, so we read back to confirm and warn if not.

    Requires the project to have been recompiled after the LookAction UPROPERTY
    was added to AAuraPlayerController — the running editor must know about the
    property, or this step is skipped with a 'recompile needed' notice."""
    bp = unreal.load_asset(blueprint_path)
    if bp is None:
        raise RuntimeError("Could not load blueprint at {}".format(blueprint_path))

    gen_class = bp.generated_class()  # method, not property, in this binding
    cdo = unreal.get_default_object(gen_class)

    # Probe whether the LookAction property exists on this build's CDO. If the
    # project hasn't been recompiled since LookAction was added to C++, the
    # running editor's class won't have it yet.
    try:
        current = cdo.get_editor_property("look_action")
    except Exception:
        _log("skip assign LookAction: the 'look_action' property is not present on "
             "the running build. Recompile the project (Build) so the new LookAction "
             "UPROPERTY is registered, then re-run this script (or set "
             "Look Action = IA_Look in BP_AuraPlayerController manually).")
        return

    if current == look_action:
        _log("skip assign LookAction on {}: already set".format(blueprint_path))
        return

    label = look_action.get_name() if look_action else "None"
    _log("assign LookAction on {} -> {}".format(blueprint_path, label))
    if not APPLY:
        return

    cdo.set_editor_property("look_action", look_action)
    bp.modify()
    unreal.EditorAssetSubsystem.get_default_object().save_asset(bp.get_path_name(), False)

    cdo2 = unreal.get_default_object(gen_class)
    confirmed = cdo2.get_editor_property("look_action")
    if confirmed == look_action:
        _log("  LookAction assignment confirmed on CDO.")
    else:
        _log("  WARNING: CDO readback did not confirm. Open BP_AuraPlayerController "
             "in the editor and set Look Action = IA_Look manually (one click), "
             "then save.")


def main():
    _log("mode: {}".format("APPLY" if APPLY else "DRY RUN (set APPLY=True to commit)"))

    ia_move = unreal.load_asset(IA_MOVE_PATH)
    if ia_move is None:
        raise RuntimeError("IA_Move not found at {}; cannot mirror value type".format(IA_MOVE_PATH))
    value_type = ia_move.value_type
    _log("IA_Move value_type = {}".format(value_type))

    ia_look = _create_input_action(IA_LOOK_PATH, value_type)
    if ia_look is None and not APPLY:
        ia_look = unreal.load_asset(IA_LOOK_PATH)

    # create_asset leaves the new package in memory only; write it to disk so it
    # survives an editor restart and shows up in source control.
    if ia_look is not None and APPLY:
        unreal.EditorAssetSubsystem.get_default_object().save_asset(ia_look.get_path_name(), False)
        _log("saved IA_Look")

    imc = unreal.load_asset(IMC_PATH)
    if imc is None:
        raise RuntimeError("IMC not found at {}".format(IMC_PATH))

    if ia_look is not None:
        for key_name in STICK_TO_MOVE:
            _add_mapping(imc, ia_move, key_name)
        for key_name in STICK_TO_LOOK:
            _add_mapping(imc, ia_look, key_name)
    else:
        _log("IA_Look not available (dry-run create skipped); skipping IMC look mappings.")

    if APPLY:
        imc.modify()
        unreal.EditorAssetSubsystem.get_default_object().save_asset(imc.get_path_name(), False)
        _log("saved IMC_AuraContext")
    else:
        _log("(dry run) would save IMC_AuraContext")

    if ia_look is not None:
        _set_bp_look_action(BP_PC_PATH, ia_look)
    else:
        _log("(dry run) IA_Look not created yet; LookAction assignment skipped.")

    _log("done.")


main()