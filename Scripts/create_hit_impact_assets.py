"""Run in UE 5.6 after compiling GAS_DemoEditor. Creates only new cue/FX assets."""
import unreal

ROOT = "/Game/GAS_Demo/GameplayCues"
FX_PATH = ROOT + "/NS_HitImpact"
BP_PATH = ROOT + "/GC_Combat_HitImpact"
SOURCE_FX = "/Game/SlashTrail_SoftTofu/Niagara/Basic/NS_Hit_Basic_Once"
SOURCE_SOUND = "/Game/SlashTrail_SoftTofu/Resource/Audio/Hit/SC_Hit_Cue"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
parent = unreal.load_class(None, "/Script/GAS_Demo.CC_GameplayCue_HitImpact")
assert parent, "Compile the C++ cue class before running this script."
assets.make_directory(ROOT)

# Never replace a user's existing asset on a repeated run.
fx = assets.load_asset(FX_PATH) if assets.does_asset_exist(FX_PATH) else None
if not fx:
    fx = assets.duplicate_asset(SOURCE_FX, FX_PATH)
    assert fx, "Could not duplicate the project's one-shot Niagara hit effect."
    assets.set_metadata_tag(fx, "HitImpact.Source", SOURCE_FX)
    assets.set_metadata_tag(fx, "HitImpact.Usage", "One-shot melee impact; local +X follows impact normal.")
    assert assets.save_loaded_asset(fx, only_if_is_dirty=False)

bp = assets.load_asset(BP_PATH) if assets.does_asset_exist(BP_PATH) else None
if not bp:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = tools.create_asset("GC_Combat_HitImpact", ROOT, unreal.Blueprint, factory)
    assert bp, "Could not create the GameplayCue Blueprint."
    defaults = unreal.get_default_object(bp.generated_class())
    defaults.set_editor_property("hit_effect", fx)
    defaults.set_editor_property("hit_sound", assets.load_asset(SOURCE_SOUND))
    defaults.set_editor_property("effect_scale", 0.65)
    defaults.set_editor_property("surface_offset", 3.0)
    defaults.set_editor_property("sound_volume", 0.7)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, only_if_is_dirty=False)

defaults = unreal.get_default_object(bp.generated_class())
assert defaults.get_editor_property("hit_effect") == fx
assert str(defaults.get_editor_property("gameplay_cue_tag").get_editor_property("tag_name")) == "GameplayCue.Combat.HitImpact"
unreal.log("HIT_IMPACT_ASSETS_OK: " + BP_PATH + " -> " + FX_PATH)
unreal.log("HIT_IMPACT_SOUND: " + str(defaults.get_editor_property("hit_sound")))
