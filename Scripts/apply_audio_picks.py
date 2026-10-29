"""UE 5.6 Python：把音效挑选台选中的声音写进全局音效配置。

用法（编辑器 Output Log 切到 Python，或 Cmd 里执行）：
    py "Scripts/apply_audio_picks.py"            应用 SourceArt/Audio/Selected/manifest.json 里的挑选结果
    py "Scripts/apply_audio_picks.py" --restore  把这些事件恢复成第一次应用之前的音效

- 导入到 /Game/GAS_Demo/Audio/Selected/<分类>/，同名资产会被覆盖（重新挑选后再运行即可更新）。
- manifest 里出现的事件，Sounds 会被**替换**为挑选结果；没出现的事件保持不变。
- 第一次运行时把这些事件原来的音效路径备份到 Saved/Audio/AudioConfig_BeforePicks.json，之后不会覆盖该备份。
"""
import json
import pathlib
import sys
import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir())
SOURCE = PROJECT / "SourceArt/Audio/Selected"
ROOT = "/Game/GAS_Demo/Audio/Selected"
CONFIG_PATH = "/Game/GAS_Demo/Audio/DA_AudioConfig_Default"
BACKUP = pathlib.Path(unreal.Paths.project_saved_dir()) / "Audio" / "AudioConfig_BeforePicks.json"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
config = assets.load_asset(CONFIG_PATH)
assert isinstance(config, unreal.CC_AudioConfig), "找不到 " + CONFIG_PATH + "，请先运行 configure_audio_starter.py"


def make_entry(sounds):
    entry = unreal.CC_AudioEntry()
    entry.set_editor_property("sounds", sounds)
    return entry


def save_and_report(entries, keys, label):
    config.set_editor_property("events", entries)
    assert assets.save_loaded_asset(config, only_if_is_dirty=False)
    for key in keys:
        assert isinstance(unreal.CC_AudioLibrary.pick_global_audio_sound(unreal.Name(key)), unreal.SoundBase), key
    unreal.log(label + " " + ", ".join(sorted(keys)))


entries = dict(config.get_editor_property("events"))

def restore():
    assert BACKUP.is_file(), "没有备份文件：" + str(BACKUP)
    saved = json.loads(BACKUP.read_text(encoding="utf-8"))
    for key, paths in saved.items():
        sounds = [assets.load_asset(p) for p in paths if assets.does_asset_exist(p)]
        if sounds:
            entries[unreal.Name(key)] = make_entry(sounds)
        else:
            entries.pop(unreal.Name(key), None)
    save_and_report(entries, [k for k, v in saved.items() if v], "AUDIO_PICKS_RESTORED")


def apply():
    rows = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))

    # 1) 导入（覆盖同名资产）
    tasks = []
    for row in rows:
        source = SOURCE / row["file"]
        assert source.is_file(), source
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", ROOT + "/" + row["category"])
        task.set_editor_property("destination_name", row["name"])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for task in tasks:
        assert task.get_editor_property("imported_object_paths"), task.get_editor_property("filename")

    # 2) 第一次应用前备份这些事件原来的音效
    grouped = {}
    for row in rows:
        sound = assets.load_asset(ROOT + "/" + row["category"] + "/" + row["name"])
        assert isinstance(sound, unreal.SoundWave), row["name"]
        grouped.setdefault(row["event"], []).append(sound)

    backup = json.loads(BACKUP.read_text(encoding="utf-8")) if BACKUP.is_file() else {}
    for key in grouped:
        if key in backup:
            continue
        old = entries.get(unreal.Name(key))
        backup[key] = [s.get_path_name() for s in old.get_editor_property("sounds") if s] if old else []
    BACKUP.parent.mkdir(parents=True, exist_ok=True)
    BACKUP.write_text(json.dumps(backup, ensure_ascii=False, indent=2), encoding="utf-8")

    # 3) 替换并校验
    for key, sounds in grouped.items():
        entries[unreal.Name(key)] = make_entry(sounds)
    save_and_report(entries, grouped.keys(), "AUDIO_PICKS_APPLIED")


restore() if "--restore" in sys.argv else apply()
