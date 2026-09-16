"""Reset the editor to a clean transient map before a validation run."""
import unreal

unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
print('RESET_VALIDATION_MAP', [str(x) for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
