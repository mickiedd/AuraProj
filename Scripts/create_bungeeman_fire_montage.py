"""Create the BungeeMan-specific LMB fire montage from the rifle animation."""

import unreal


SOURCE = "/Game/BungeeMan/Animations/Fire_Rifle_Hip.Fire_Rifle_Hip"
PACKAGE_PATH = "/Game/BungeeMan/Animations"
ASSET_NAME = "AM_BungeeMan_FireGun"
OBJECT_PATH = PACKAGE_PATH + "/" + ASSET_NAME + "." + ASSET_NAME


def main():
    source = unreal.load_object(None, SOURCE)
    if not source:
        raise RuntimeError("Missing source animation: {}".format(SOURCE))
    existing = unreal.load_object(None, OBJECT_PATH)
    if existing:
        montage = existing
        print("existing montage={}".format(montage))
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", source)
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        montage = tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.AnimMontage, factory)
        if not montage:
            raise RuntimeError("Could not create {}".format(OBJECT_PATH))
        print("created montage={}".format(montage))

    print("montage class={} skeleton={} length={}".format(
        montage.get_class().get_name(), montage.get_skeleton(), montage.sequence_length))
    print("source skeleton={} length={}".format(source.get_skeleton(), source.sequence_length))
    unreal.EditorLoadingAndSavingUtils.save_packages([montage.get_outer()], False)
    print("saved {}".format(OBJECT_PATH))


main()
