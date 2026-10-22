import unreal

ROOT = '/Game/GAS_Demo/Enemy/Warrior/Materials'
assets = unreal.EditorAssetLibrary
task = unreal.AssetImportTask()
task.filename = unreal.Paths.project_content_dir() + 'GAS_Demo/Enemy/Warrior/Materials/Source/PolyArtTex_RedGold.png'
task.destination_path = ROOT
task.destination_name = 'T_PolyArt_RedGold'
task.automated = True
task.replace_existing = False
task.save = True
if not assets.does_asset_exist(ROOT + '/T_PolyArt_RedGold'):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(ROOT + '/T_PolyArt_RedGold')
original = unreal.load_asset('/Game/ModularRPGHeroesPolyart/Textures/PolyArtTex')
assert texture and original
for property_name in ['srgb', 'filter', 'address_x', 'address_y', 'compression_settings', 'lod_group']:
    texture.set_editor_property(property_name, original.get_editor_property(property_name))
instance = assets.duplicate_asset(ROOT + '/MI_Warrior_Red', ROOT + '/MI_Warrior_RedGold')
assert instance
parameter = unreal.TextureParameterValue()
parameter.set_editor_property('parameter_info', unreal.MaterialParameterInfo(name='BaseColorTexture'))
parameter.set_editor_property('parameter_value', texture)
instance.set_editor_property('texture_parameter_values', [parameter])
unreal.MaterialEditingLibrary.update_material_instance(instance)
assert assets.save_loaded_asset(instance)
assert instance.get_editor_property('texture_parameter_values')[0].get_editor_property('parameter_value') == texture
unreal.log('POLYART_RED_GOLD_SUCCESS ' + instance.get_path_name())
