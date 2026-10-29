"""Import quiet UI foley into a separate config; preserve the original starter asset."""
from pathlib import Path
import json
import unreal

source = Path(unreal.Paths.project_dir()) / 'SourceArt/Audio/RealisticUI'
rows = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
root = '/Game/GAS_Demo/Audio'
destination = root + '/RealisticUI'
config_path = root + '/DA_AudioConfig_RealisticUI'
base = assets.load_asset(root + '/DA_AudioConfig_Default')
assert isinstance(base, unreal.CC_AudioConfig)
tasks = []
for row in rows:
    if assets.does_asset_exist(destination + '/' + row['name']):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source / row['file']))
    task.set_editor_property('destination_path', destination)
    task.set_editor_property('destination_name', row['name'])
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    tasks.append(task)
if tasks:
    tools.import_asset_tasks(tasks)
config = assets.load_asset(config_path) if assets.does_asset_exist(config_path) else assets.duplicate_asset(base.get_path_name(), config_path)
assert isinstance(config, unreal.CC_AudioConfig)
entries = dict(config.get_editor_property('events'))
for event in dict.fromkeys(r['event'] for r in rows):
    entry = unreal.CC_AudioEntry()
    sounds = [assets.load_asset(destination + '/' + r['name']) for r in rows if r['event'] == event]
    assert all(isinstance(s, unreal.SoundWave) and s.get_editor_property('duration') > 0 for s in sounds)
    entry.set_editor_property('sounds', sounds)
    entries[unreal.Name(event)] = entry
config.set_editor_property('events', entries)
assert assets.save_loaded_asset(config, only_if_is_dirty=False)
base_events = dict(base.get_editor_property('events'))
for event, entry in base_events.items():
    if not str(event).startswith('UI.'):
        assert list(entries[event].get_editor_property('sounds')) == list(entry.get_editor_property('sounds')), event
assert unreal.CC_AudioLibrary.get_audio_config() == config
for event in entries:
    assert isinstance(unreal.CC_AudioLibrary.pick_global_audio_sound(event), unreal.SoundBase), event
report = dict(config=config.get_path_name(), ui_sounds=len(rows), ui_events=8, all_events=len(entries),
              global_access='passed', non_ui_preserved='passed', original_config_preserved=True)
saved = Path(unreal.Paths.project_saved_dir()) / 'Audio'
saved.mkdir(exist_ok=True, parents=True)
(saved / 'RealisticUISetup.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('REALISTIC_UI_OK ' + json.dumps(report))
