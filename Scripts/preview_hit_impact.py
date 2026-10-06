"""Open the created Niagara asset in the editor after validating the cue defaults."""
import runpy
import unreal

runpy.run_path(unreal.Paths.project_dir() + "Scripts/create_hit_impact_assets.py")
fx = unreal.load_asset("/Game/GAS_Demo/GameplayCues/NS_HitImpact")
bp = unreal.load_asset("/Game/GAS_Demo/GameplayCues/GC_Combat_HitImpact")
cue = unreal.get_default_object(bp.generated_class())
assert not cue.on_execute(None, unreal.GameplayCueParameters())
unreal.EditorAssetLibrary.sync_browser_to_objects([bp.get_path_name(), fx.get_path_name()])
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([fx])
unreal.log("HIT_IMPACT_PREVIEW_READY")
