import unreal

ROOT = '/Game/GAS_Demo/Enemy/Warrior/Materials'
SOURCE = 'D:/UEProject/GAS_Demo/Content/GAS_Demo/Enemy/Warrior/Materials/Source/PolyArtTex_Red.png'
assets = unreal.EditorAssetLibrary
editing = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
material = assets.duplicate_asset('/Game/ModularRPGHeroesPolyart/Materials/PolyartMat', ROOT + '/M_Polyart_RedBase')
original = unreal.load_asset('/Game/ModularRPGHeroesPolyart/Textures/PolyArtTex')
assert material and original
assets.make_directory(ROOT)
task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = ROOT
task.destination_name = 'T_PolyArt_Red'
task.automated = True
task.replace_existing = False
task.save = True
if not assets.does_asset_exist(ROOT + '/T_PolyArt_Red'):
    tools.import_asset_tasks([task])
texture = unreal.load_asset(ROOT + '/T_PolyArt_Red')
assert texture
for property_name in ['srgb', 'filter', 'address_x', 'address_y', 'compression_settings', 'lod_group']:
    texture.set_editor_property(property_name, original.get_editor_property(property_name))
assets.save_loaded_asset(texture)
if 'BaseColorTexture' not in [str(name) for name in editing.get_texture_parameter_names(material)]:
    expression = editing.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, -160)
    expression.set_editor_property('parameter_name', 'BaseColorTexture')
    expression.set_editor_property('texture', original)
    assert editing.connect_material_property(expression, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    editing.recompile_material(material)
    assets.save_loaded_asset(material)
instance_path = ROOT + '/MI_Warrior_Red'
assert not assets.does_asset_exist(instance_path), 'Refusing to overwrite existing material instance'
instance = tools.create_asset('MI_Warrior_Red', ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
editing.set_material_instance_parent(instance, material)
parameter = unreal.TextureParameterValue()
parameter.set_editor_property('parameter_info', unreal.MaterialParameterInfo(name='BaseColorTexture'))
parameter.set_editor_property('parameter_value', texture)
instance.set_editor_property('texture_parameter_values', [parameter])
editing.update_material_instance(instance)
assets.save_loaded_asset(instance)
assert instance.get_editor_property('parent') == material
assert instance.get_editor_property('texture_parameter_values')[0].get_editor_property('parameter_value') == texture
unreal.log('POLYART_RED_SUCCESS ' + instance.get_path_name())
