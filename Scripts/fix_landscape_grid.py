# fix_landscape_grid.py
# Run inside Unreal Editor's Python (console:  py fix_landscape_grid.py).
# Snaps the Landscape actors in L_showcase_level onto a flush grid so there
# are no gaps or overlaps between tiles.
#
# Strategy: every tile is ~101600 cm (1016 m) square. The center tile (the one
# whose bbox center is closest to the world origin) is treated as the anchor
# and is assumed to be correctly placed; every other tile is moved to the
# nearest grid node (anchor + k*W) so all tile edges coincide exactly.
# Z is preserved — only XY alignment is corrected (gaps/overlaps are XY).
#
# IMPORTANT:
#   APPLY = False  -> dry run: prints the planned moves and verifies nothing.
#   APPLY = True   -> moves the actors and writes a revert JSON you can use to
#                     undo. Re-run with APPLY=False to see the plan first.
#
# After applying, SAVE THE LEVEL (Ctrl+S) to persist the moves.

import json
import unreal

LEVEL_PATH = "/Game/Scifi_desert_city/Level/L_showcase_level"
APPLY = True
REVERT_FILE_NAME = "Saved/Scripts/landscape_grid_revert.json"


def load_world():
    try:
        return unreal.EditorLoadingAndSavingUtils.load_map(unreal.PackagePath(LEVEL_PATH), False, False)
    except Exception:
        try:
            return unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH, False, False)
        except Exception:
            try:
                return unreal.EditorLevelLibrary.load_editor_level(LEVEL_PATH)
            except Exception:
                return unreal.EditorLevelLibrary.get_editor_world()


def actor_bbox_xy(a):
    # (min_x, min_y, max_x, max_y) in world space
    o, e = a.get_actor_bounds(only_colliding_components=False)
    return (o.x - e.x, o.y - e.y, o.x + e.x, o.y + e.y)


def snap(v, anchor, W):
    return anchor + W * round((v - anchor) / W)


def main():
    unreal.log("==== fix_landscape_grid start (APPLY={}) ====".format(APPLY))
    world = load_world()
    if world is None:
        unreal.log_error("Could not load level " + LEVEL_PATH)
        return

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    lands = [a for a in actors if a.get_class().get_name().startswith("Landscape")]
    if len(lands) < 2:
        unreal.log_warning("Found {} landscape actors; nothing to grid.".format(len(lands)))
        return
    unreal.log("Found {} landscape actors.".format(len(lands)))

    # Per-tile bbox; median width = grid pitch W.
    bbs = {a.get_name(): actor_bbox_xy(a) for a in lands}
    widths = sorted(b[2] - b[0] for b in bbs.values())
    W = widths[len(widths) // 2]
    # Sanity: tiles should all be ~the same width. Warn if not.
    if max(widths) - min(widths) > 100.0:
        unreal.log_warning("Tile widths vary ({:.0f}..{:.0f}); snapping to median W={:.0f}.".format(
            min(widths), max(widths), W))

    # Anchor = the tile whose bbox center is closest to the world origin.
    def center(b):
        return ((b[0] + b[2]) / 2.0, (b[1] + b[3]) / 2.0)
    names = list(bbs.keys())
    anchor_name = min(names, key=lambda n: center(bbs[n])[0] ** 2 + center(bbs[n])[1] ** 2)
    anchor_actor = next(a for a in lands if a.get_name() == anchor_name)
    aloc = anchor_actor.get_actor_location()
    ax, ay = aloc.x, aloc.y
    unreal.log("Grid pitch W={:.0f}  anchor={}  loc=({:.0f},{:.0f})".format(W, anchor_name, ax, ay))

    revert = {}
    for a in lands:
        name = a.get_name()
        loc = a.get_actor_location()
        nx = snap(loc.x, ax, W)
        ny = snap(loc.y, ay, W)
        nz = loc.z  # preserve Z
        dx, dy = nx - loc.x, ny - loc.y
        if abs(dx) < 1.0 and abs(dy) < 1.0:
            unreal.log("  {:18s} already aligned (dx={:+.0f} dy={:+.0f})".format(name, dx, dy))
            continue
        revert[name] = [loc.x, loc.y, loc.z]
        unreal.log("  {:18s} ({:9.0f},{:9.0f},{:6.0f}) -> ({:9.0f},{:9.0f},{:6.0f})  dx={:+.0f} dy={:+.0f}".format(
            name, loc.x, loc.y, loc.z, nx, ny, nz, dx, dy))
        if not APPLY:
            continue
        moved = False
        try:
            # teleport=True so the move isn't blocked by collision/sweep
            moved = a.set_actor_location(unreal.Vector(nx, ny, nz), False, True)
        except Exception:
            try:
                a.set_actor_location(unreal.Vector(nx, ny, nz))
                moved = True
            except Exception as e:
                unreal.log_error("  failed to move {}: {}".format(name, e))
        if not moved:
            unreal.log_error("  set_actor_location returned False for {}".format(name))
            continue
        # verify
        after = a.get_actor_location()
        if abs(after.x - nx) > 1.0 or abs(after.y - ny) > 1.0:
            unreal.log_warning("  {} did not stick (now at {:.0f},{:.0f})".format(name, after.x, after.y))
        else:
            unreal.log("  moved {}.".format(name))

    if not revert:
        unreal.log("Nothing to move — all tiles already aligned.")
    elif APPLY:
        proj = unreal.SystemLibrary.get_project_directory()
        import os
        path = os.path.join(proj, REVERT_FILE_NAME)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            json.dump(revert, f, indent=2)
        unreal.log("Wrote revert data: {} ({} actors). Set APPLY=False and re-run to re-verify.".format(
            path, len(revert)))
        unreal.log("Remember to SAVE THE LEVEL (Ctrl+S) to persist.")
    else:
        unreal.log("DRY RUN — no changes made. Set APPLY=True at the top of the script and re-run to apply.")
    unreal.log("==== fix_landscape_grid done ====")


main()