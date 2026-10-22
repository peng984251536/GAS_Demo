import unreal

material = unreal.load_asset('/Game/ModularRPGHeroesPolyart/Materials/PolyartMat')
unreal.log('POLYART_MATERIAL ' + material.get_path_name())
unreal.log('POLYART_PARAMETERS ' + str(unreal.MaterialEditingLibrary.get_texture_parameter_names(material)))
for texture in [unreal.load_asset('/Game/ModularRPGHeroesPolyart/Textures/PolyArtTex')]:
    unreal.log('POLYART_TEXTURE ' + texture.get_path_name())
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = unreal.Paths.project_saved_dir() + 'PolyartRed/' + texture.get_name() + '.tga'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterTGA()
    unreal.log('POLYART_EXPORT ' + str(unreal.Exporter.run_asset_export_task(task)))
