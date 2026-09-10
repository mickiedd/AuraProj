"""Idempotently author entrance markers for the showcase landmark actors.

Run from Unreal's Python console or with -ExecutePythonScript after opening the
showcase map. The script never moves the source building and refuses to save if
an existing marker has a conflicting ID. Navigation projection is left to the
runtime registry, where the current world nav data is available.
"""
import unreal

MAP = "/Game/Scifi_desert_city/Level/L_showcase_level"
MARKER_CLASS = "/Script/Aura.AuraLandmarkMarker"
LANDMARKS = {
    "ZhenhaiTower": ("Zhenhai Tower", 10),
    "GreatNorthGate": ("Great North Gate", 20),
}


def _transform(location, rotation):
    return unreal.Transform(location=location, rotation=rotation, scale=unreal.Vector(1, 1, 1))


def author():
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    marker_class = unreal.load_class(None, MARKER_CLASS)
    if not marker_class:
        raise RuntimeError("AuraLandmarkMarker native class is unavailable; build the Aura module first")

    markers = {}
    sources = {}
    for actor in actors:
        if actor.get_class().get_name() == "AuraLandmarkMarker":
            marker_id = actor.get_editor_property("landmark_id")
            if marker_id in markers:
                raise RuntimeError("duplicate authored landmark marker: %s" % marker_id)
            markers[marker_id] = actor
        for tag, metadata in LANDMARKS.items():
            if actor.actor_has_tag(tag):
                if tag in sources:
                    raise RuntimeError("duplicate source landmark tag: %s" % tag)
                sources[tag] = actor

    changed = []
    for tag, (display_name, sort_order) in LANDMARKS.items():
        source = sources.get(tag)
        if not source:
            unreal.log_warning("[Landmark] source actor missing for %s" % tag)
            continue
        marker = markers.get(tag)
        if not marker:
            marker = unreal.EditorLevelLibrary.spawn_actor_from_class(marker_class, source.get_actor_location())
            marker.set_actor_label("Landmark_%s" % tag)
            marker.set_folder_path("ScifiDesert/Landmarks")
            markers[tag] = marker
        bounds_origin, bounds_extent = source.get_actor_bounds(only_colliding_components=True)
        # The imported showcase buildings face the approach lane on negative Y.
        approach = bounds_origin + unreal.Vector(0, -(bounds_extent.y + 300), 0)
        marker.set_editor_property("landmark_id", tag)
        marker.set_editor_property("display_name", unreal.Text(display_name))
        marker.set_editor_property("sort_order", sort_order)
        marker.set_editor_property("b_enabled", True)
        marker.set_editor_property("approach_transform", _transform(approach, (bounds_origin - approach).rotation()))
        marker.set_editor_property("arrival_radius", 150.0)
        changed.append(tag)

    if changed:
        unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[Landmark] authored markers: %s" % ", ".join(changed or ["none"]))
    return changed


if __name__ == "__main__":
    author()
