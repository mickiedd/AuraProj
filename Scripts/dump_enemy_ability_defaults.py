import json
import unreal


ABILITY_PATHS = [
    "/Game/Blueprints/AbilitySystem/Enemy/Abilities/GA_EnemyFireBolt",
    "/Game/Blueprints/AbilitySystem/Enemy/Abilities/GA_RangedAttack",
    "/Game/Blueprints/AbilitySystem/Enemy/Abilities/GA_MeleeAttack",
    "/Game/Blueprints/AbilitySystem/Enemy/Abilities/GA_HitReact",
]


def value(obj, name):
    try:
        return str(obj.get_editor_property(name))
    except Exception:
        return None


result = {}
for path in ABILITY_PATHS:
    blueprint = unreal.EditorAssetLibrary.load_asset(path)
    generated = blueprint.generated_class()
    default = unreal.get_default_object(generated)
    result[path] = {
        name: value(default, name)
        for name in [
            "ability_tags", "activation_owned_tags", "startup_input_tag",
            "damage_type", "damage", "debuff_chance", "debuff_damage",
            "debuff_duration", "debuff_frequency", "death_impulse_magnitude",
            "knockback_force_magnitude", "knockback_chance", "projectile_class",
            "should_override_pitch", "pitch_override", "cost_gameplay_effect_class",
            "cooldown_gameplay_effect_class",
        ]
    }

class_info = unreal.EditorAssetLibrary.load_asset(
    "/Game/Blueprints/AbilitySystem/Data/DA_CharacterClassInfo"
)
result["character_class_info"] = {
    "common_abilities": value(class_info, "common_abilities"),
    "character_class_information": value(class_info, "character_class_information"),
}

output = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_saved_dir() + "AuraMigration/EnemyAbilityDefaults.json"
)
unreal.SystemLibrary.make_directory(unreal.Paths.get_path(output))
with open(output, "w", encoding="utf-8") as handle:
    json.dump(result, handle, indent=2)
unreal.log("Wrote " + output)
