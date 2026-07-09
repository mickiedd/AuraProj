# diag_gamepad_input.py  — runtime gamepad diagnostic against a LIVE PIE session.
# Run: python Scripts/remote_run.py Scripts/diag_gamepad_input.py
# Push + HOLD the left stick (and right stick) during the ~6s poll window.

import time
import unreal

POLL_SECONDS = 15.0
POLL_INTERVAL = 0.2

IA_MOVE = unreal.load_object(None, "/Game/Blueprints/Input/InputActions/IA_Move.IA_Move")
IA_LOOK = unreal.load_object(None, "/Game/Blueprints/Input/InputActions/IA_Look.IA_Look")
IMC = unreal.load_object(None, "/Game/Blueprints/Input/IMC_AuraContext.IMC_AuraContext")

CANDIDATE_AXIS_KEYS = [
    "Gamepad_LeftStickX", "Gamepad_LeftStickY",
    "Gamepad_RightStickX", "Gamepad_RightStickY",
    "Gamepad_LeftTriggerAxis", "Gamepad_RightTriggerAxis",
]


def _log(m):
    print("[diag] " + m)


def _make_key(name):
    k = unreal.Key()
    try:
        k.set_editor_property("key_name", name)
    except Exception:
        return None
    return k


def _get_subsystem(pc, gw):
    """Try several ways to reach UEnhancedInputLocalPlayerSubsystem."""
    lp = None
    for getter in ("get_local_player",):
        try:
            lp = getattr(pc, getter)()
            if lp:
                break
        except Exception:
            pass
    if lp is None:
        try:
            lp = pc.get_editor_property("player")
        except Exception:
            lp = None
    if lp is None:
        try:
            gi = unreal.GameplayStatics.get_game_instance(gw)
            if gi:
                lp = gi.get_local_player_by_index(0)
        except Exception as e:
            _log("  gi.get_local_player_by_index err: {}".format(str(e)[:80]))
    if lp is None:
        _log("  could not obtain local player")
        return None
    _log("  local player: {}".format(lp))
    for attempt, fn in (("lp.get_subsystem", lambda: lp.get_subsystem(unreal.EnhancedInputLocalPlayerSubsystem)),
                       ("static .get", lambda: unreal.EnhancedInputLocalPlayerSubsystem.get(lp))):
        try:
            s = fn()
            if s is not None:
                _log("  subsystem via {} OK".format(attempt))
                return s
        except Exception as e:
            _log("  {} err: {}".format(attempt, str(e)[:80]))
    return None


def dump_imc_mappings():
    _log("==== IMC asset mappings (action / key / modifiers) ====")
    try:
        maps = IMC.get_mappings()
    except Exception as e:
        _log("  get_mappings err: {}".format(str(e)[:80]))
        return
    _log("  mapping count: {}".format(len(maps)))
    for m in maps:
        try:
            act = m.get_editor_property("action")
            key = m.get_editor_property("key")
            act_name = act.get_name() if act else "(none)"
            key_name = "?"
            try:
                key_name = key.get_editor_property("key_name")
            except Exception:
                pass
            mods = m.get_editor_property("modifiers")
            mod_names = []
            for mod in mods:
                try:
                    mod_names.append(mod.get_class().get_name())
                except Exception:
                    mod_names.append("?")
            _log("  action={} key={} mods={}".format(act_name, key_name, mod_names))
        except Exception as e:
            _log("  mapping read err: {}".format(str(e)[:80]))


def main():
    dump_imc_mappings()

    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    gw = ues.get_game_world()
    _log("game world (PIE): {}".format(gw))
    if gw is None:
        _log("No PIE running. Start Play In Editor, then re-run.")
        return
    pc = unreal.GameplayStatics.get_player_controller(gw, 0)
    _log("PC0: {} class={}".format(pc, pc.get_class().get_name() if pc else "?"))
    if pc is None:
        return

    subsys = _get_subsystem(pc, gw)
    if subsys is not None:
        try:
            _log("  has_mapping_context(AuraContext): {}".format(subsys.has_mapping_context(IMC)))
        except Exception as e:
            _log("  has_mapping_context err: {}".format(str(e)[:80]))
        for act, label in ((IA_MOVE, "IA_Move"), (IA_LOOK, "IA_Look")):
            if act is None:
                continue
            try:
                keys = subsys.query_keys_mapped_to_action(act)
                names = []
                for kk in keys:
                    try:
                        names.append(kk.get_editor_property("key_name"))
                    except Exception:
                        names.append("?")
                _log("  {} mapped keys (runtime): {}".format(label, names))
            except Exception as e:
                _log("  {} query err: {}".format(label, str(e)[:80]))

    _log("==== RAW GAMEPAD POLL: push+HOLD left & right sticks for {}s ====".format(POLL_SECONDS))
    end = time.time() + POLL_SECONDS
    max_seen = {}
    while time.time() < end:
        line = []
        for kn in CANDIDATE_AXIS_KEYS:
            k = _make_key(kn)
            if k is None:
                continue
            try:
                v = float(pc.get_input_analog_key_state(k))
            except Exception:
                v = 0.0
            if abs(v) > 0.02:
                line.append("{}={:+.3f}".format(kn, v))
                max_seen[kn] = max(max_seen.get(kn, 0.0), abs(v))
        if line:
            _log("  " + "  ".join(line))
        time.sleep(POLL_INTERVAL)

    _log("==== poll done. Max |value| per key: ====")
    if not max_seen:
        _log("  NONE > 0.02 — sticks produce no axis in UE (BetterJoy not outputting sticks, or deadzone).")
    else:
        for kn, mv in sorted(max_seen.items(), key=lambda x: -x[1]):
            _log("  {} = {:.3f}".format(kn, mv))
    _log("done.")


main()