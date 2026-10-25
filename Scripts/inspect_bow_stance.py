import unreal

tree = unreal.load_asset('/Game/GAS_Demo/Enemy/BowStance/AI/AI_BowStance')
assert tree
task = unreal.AssetExportTask()
task.object = tree
task.filename = unreal.Paths.project_saved_dir() + 'bow_stance_tree.t3d'
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.ObjectExporterT3D()
assert unreal.Exporter.run_asset_export_task(task)
unreal.log('BOW_TREE_EXPORTED')
for asset_path in ['/Game/GAS_Demo/Enemy/BowStance/AI/BTD_Distance', '/Game/GAS_Demo/Enemy/BowStance/AI/BTD_LostTarget', '/Game/GAS_Demo/AI/BehaviorTree/BTS_UpdateData']:
    blueprint = unreal.load_asset(asset_path)
    defaults = unreal.get_default_object(blueprint.generated_class())
    for name in ['flow_abort_mode', 'inverse_condition', 'interval', 'random_deviation']:
        try:
            unreal.log('BOW_DEFAULT ' + blueprint.get_name() + ' ' + name + '=' + str(defaults.get_editor_property(name)))
        except Exception:
            pass
