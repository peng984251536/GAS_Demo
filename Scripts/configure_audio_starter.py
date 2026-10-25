"""UE 5.6 Python commandlet: import the starter WAVs and create the default audio asset.

Run after building GAS_DemoEditor. Only adds assets/entries that do not exist;
existing audio and edited configuration entries are preserved.
"""
import json
import pathlib
import re
import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir())
SOURCE = PROJECT / "SourceArt/Audio/CartoonStarterKit"
ROOT = "/Game/GAS_Demo/Audio"
CONFIG_PATH = ROOT + "/DA_AudioConfig_Default"
rows = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

tasks = []
for row in rows:
    path = ROOT + "/" + row["category"] + "/" + row["name"]
    if assets.does_asset_exist(path):
        assert isinstance(assets.load_asset(path), unreal.SoundBase), path
        continue
    source = SOURCE / row["file"]
    assert source.is_file(), source
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", ROOT + "/" + row["category"])
    task.set_editor_property("destination_name", row["name"])
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    tasks.append(task)
if tasks:
    tools.import_asset_tasks(tasks)
for task in tasks:
    assert task.get_editor_property("imported_object_paths"), task.get_editor_property("filename")

assets.make_directory(ROOT)
config = assets.load_asset(CONFIG_PATH) if assets.does_asset_exist(CONFIG_PATH) else None
if config is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.CC_AudioConfig.static_class())
    config = tools.create_asset("DA_AudioConfig_Default", ROOT, unreal.CC_AudioConfig, factory)
assert isinstance(config, unreal.CC_AudioConfig), CONFIG_PATH

grouped = {}
for row in rows:
    prefix = re.sub(r"_\d+$", "", row["name"].removeprefix("SFX_"))
    key = prefix.replace("_", ".")
    sound = assets.load_asset(ROOT + "/" + row["category"] + "/" + row["name"])
    assert isinstance(sound, unreal.SoundWave), row["name"]
    grouped.setdefault(key, []).append(sound)

entries = dict(config.get_editor_property("events"))
added = 0
for key, sounds in grouped.items():
    event = unreal.Name(key)
    if event in entries:
        continue
    entry = unreal.CC_AudioEntry()
    entry.set_editor_property("sounds", sounds)
    entries[event] = entry
    added += 1
config.set_editor_property("events", entries)
assert assets.save_loaded_asset(config, only_if_is_dirty=False)

# Validate the actual project-settings lookup and the reflected global BP function.
assert unreal.CC_AudioLibrary.get_audio_config() == config, "Project settings did not resolve the default asset"
for key in grouped:
    selected = unreal.CC_AudioLibrary.pick_global_audio_sound(unreal.Name(key))
    assert isinstance(selected, unreal.SoundBase), key
assert unreal.CC_AudioLibrary.pick_global_audio_sound(unreal.Name("Missing.Event")) is None

# Events is EditDefaultsOnly. Test on the native defaults and restore them;
# no temporary test asset or changes to the saved audio asset are needed.
temporary = unreal.get_default_object(unreal.CC_AudioConfig)
original_events = dict(temporary.get_editor_property("events"))
try:
    empty = unreal.CC_AudioEntry()
    empty.set_editor_property("sounds", [None])
    temporary.set_editor_property("events", {unreal.Name("Test.Empty"): empty})
    assert temporary.pick_sound(unreal.Name("Test.Empty")) is None
    valid = unreal.CC_AudioEntry()
    valid.set_editor_property("sounds", [None, selected, None])
    temporary.set_editor_property("events", {unreal.Name("Test.Valid"): valid})
    for _ in range(8):
        assert temporary.pick_sound(unreal.Name("Test.Valid")) == selected
finally:
    temporary.set_editor_property("events", original_events)

report = {"config": config.get_path_name(), "sounds": len(rows), "events": len(entries),
          "imported_this_run": len(tasks), "added_events": added,
          "global_lookup": "passed", "global_selection": "passed", "empty_and_null_variants": "passed",
          "playback_wiring": "not changed"}
saved = pathlib.Path(unreal.Paths.project_saved_dir()) / "Audio"
saved.mkdir(parents=True, exist_ok=True)
(saved / "AudioConfigSetup.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("AUDIO_CONFIG_SETUP_OK " + json.dumps(report, ensure_ascii=False))
