import json
import unreal

def property_value(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None

def node_info(node):
    if not node:
        return None
    result = {'class': node.get_class().get_name(), 'object': node.get_path_name()}
    for name in ['node_name', 'flow_abort_mode', 'blackboard_key', 'key_query', 'operation_type', 'basic_operation', 'wait_time', 'random_deviation', 'acceptable_radius', 'target_key', 'gameplay_tag', 'tag', 'ability_tag', 'target_actor', 'is_hit', 'is_dead', 'b_key_dead', 'b_key_hit', 'b_key_target_actor', 'b_key_last_known_location']:
        value = property_value(node, name)
        if value is not None:
            result[name] = str(value)
    for name in ['services', 'decorators']:
        value = property_value(node, name)
        if value is not None:
            result[name] = [node_info(item) for item in value]
    children = property_value(node, 'children')
    if children is not None:
        result['children'] = []
        for child in children:
            result['children'].append({
                'decorators': [node_info(item) for item in child.get_editor_property('decorators')],
                'composite': node_info(child.get_editor_property('child_composite')),
                'task': node_info(child.get_editor_property('child_task')),
            })
    return result

tree = unreal.load_asset('/Game/GAS_Demo/Enemy/Warrior/AI/AI_MeleeStance')
task = unreal.AssetExportTask()
task.object = tree
task.filename = unreal.Paths.project_saved_dir() + 'melee_stance_tree.t3d'
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.ObjectExporterT3D()
assert unreal.Exporter.run_asset_export_task(task)
unreal.log('MELEE_TREE_EXPORTED')
for asset_path in ['/Game/GAS_Demo/Enemy/BowStance/AI/BTD_LostTarget', '/Game/GAS_Demo/AI/BehaviorTree/BTT_TryActiveAttackAbility', '/Game/GAS_Demo/AI/BehaviorTree/BTS_UpdateData', '/Game/GAS_Demo/AI/BehaviorTree/BTT_ActiveAbility']:
    task.object = unreal.load_asset(asset_path)
    task.filename = unreal.Paths.project_saved_dir() + asset_path.rsplit('/', 1)[-1] + '.t3d'
    unreal.Exporter.run_asset_export_task(task)
    defaults = unreal.get_default_object(task.object.generated_class())
    for name in ['interval', 'random_deviation', 'flow_abort_mode', 'inverse_condition', 'check_condition_only_black_board_changes', 'restart_timer_on_each_activation', 'call_tick_on_search_start']:
        unreal.log('MELEE_DEFAULT ' + task.object.get_name() + ' ' + name + '=' + str(property_value(defaults, name)))
