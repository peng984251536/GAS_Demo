"""诊断 Lvl_TopDown 里"放置的 actor 显示为 Unloaded / 变灰"的原因。

用途
----
World Partition 关卡里 actor "还在 Outliner 但变灰(Unloaded)"可能来自四种原因，
本脚本把它们逐条打出来，避免靠猜：

  1. 关卡侧：UWorldPartition 是否启用流送、运行时网格的 CellSize / LoadingRange
  2. 世界边界：actor 是否落在 UWorldPartition 的 WorldBounds 之外（落在外面就无法归入任何单元格）
  3. actor 侧：bIsSpatiallyLoaded 是否与关卡里其它 actor 不一致
  4. Data Layer：actor 是否被分到了一个未激活的 Data Layer

输出
----
Saved/WorldPartitionDiagnosis/diagnosis.json 以及 unreal.log 里的可读摘要。

运行方式（二选一）
------------------
A. 编辑器里打开 Lvl_TopDown，然后 Tools -> Execute Python Script 选本文件。
B. 命令行（会自动 load_map）：
   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<本文件> \
       -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities \
       -unattended -nosplash -nullrhi -abslog=<日志路径>

注意：UE 的 Python 绑定在不同版本里属性名会变（bIsSpatiallyLoaded 可能暴露为
is_spatially_loaded / b_is_spatially_loaded），所以下面一律先探测再读取，读不到就
把候选名字列出来，而不是直接抛异常。
"""

import json
from pathlib import Path

import unreal

TARGET_MAP = "/Game/GAS_Demo/Map/Lvl_TopDown"
OUT_DIR = Path(unreal.Paths.project_saved_dir()) / "WorldPartitionDiagnosis"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# 关键属性名的候选写法；不同 UE 版本暴露的名字不一样。
SPATIALLY_LOADED_NAMES = ("is_spatially_loaded", "b_is_spatially_loaded", "bIsSpatiallyLoaded")
EDITOR_ONLY_NAMES = ("is_editor_only_actor", "b_is_editor_only_actor", "bIsEditorOnlyActor")
RUNTIME_GRID_NAMES = ("runtime_grid", "runtime_grid_name", "b_is_runtime_grid_set")


def note(msg):
    unreal.log("[WP诊断] " + str(msg))


def try_call(obj, name, *args):
    """调用 obj.name(*args)；不存在或抛异常则返回 (False, 错误文本)。"""
    fn = getattr(obj, name, None)
    if fn is None:
        return False, "no attribute " + name
    try:
        return True, fn(*args)
    except Exception as exc:
        return False, "%s: %s" % (name, exc)


def probe(obj, candidates):
    """按候选名依次尝试 get_editor_property / 属性访问，返回第一个成功的结果。"""
    if obj is None:
        return None, "object is None"
    for name in candidates:
        try:
            return obj.get_editor_property(name), name
        except Exception:
            pass
        try:
            return getattr(obj, name), name
        except Exception:
            pass
    return None, "none of %s" % (", ".join(candidates),)


def list_attrs(obj, keywords):
    """把 obj 上名字含任一关键词的属性/方法列出来——用于探测未知的 Python 绑定。"""
    if obj is None:
        return []
    found = []
    for name in dir(obj):
        lowered = name.lower()
        if any(k in lowered for k in keywords):
            found.append(name)
    return sorted(found)


def get_editor_world():
    ok, world = try_call(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem), "get_editor_world")
    if ok and world:
        return world
    ok, world = try_call(unreal.EditorLevelLibrary, "get_editor_world")
    if ok and world:
        return world
    return None


def get_all_actors(world):
    ok, actors = try_call(unreal.EditorLevelLibrary, "get_all_level_actors")
    if ok and actors:
        return list(actors)
    ok, actors = try_call(unreal.GameplayStatics, "get_all_actors_of_class", world, unreal.Actor)
    if ok and actors:
        return list(actors)
    return []


report = {}

# ---------------------------------------------------------------- 定位世界
world = get_editor_world()
world_name = world.get_name() if world else None
report["editor_world"] = world_name
note("当前编辑器世界: %s" % world_name)

if world_name != "Lvl_TopDown":
    note("当前不是 Lvl_TopDown，尝试 load_map(%s) ..." % TARGET_MAP)
    ok, res = try_call(unreal.EditorLoadingAndSavingUtils, "load_map", TARGET_MAP)
    note("load_map -> ok=%s %s" % (ok, "" if ok else res))
    world = get_editor_world()
    world_name = world.get_name() if world else None
    report["editor_world_after_load"] = world_name

if not world:
    note("拿不到编辑器世界，退出。")
    raise SystemExit(1)

# ---------------------------------------------------------------- 关卡侧 WP 配置
wp = None
value, src = probe(world, ("world_partition", "WorldPartition"))
if value is not None:
    wp = value
    report["world_partition_from"] = src

report["world_partition_probe"] = {
    "found": wp is not None,
    "attrs_matching_keywords": list_attrs(wp, ("stream", "grid", "cell", "bound", "partition")),
}
if wp is not None:
    for label, candidates in (
        ("bEnableStreaming", ("b_enable_streaming", "bEnableStreaming")),
        ("bEnableWorldBoundsChecks", ("b_enable_world_bounds_checks", "bEnableWorldBoundsChecks")),
        ("WorldBounds", ("world_bounds", "WorldBounds")),
        ("RuntimeHash", ("runtime_hash", "RuntimeHash")),
    ):
        value, src = probe(wp, candidates)
        report["world_partition_probe"][label] = {"value": str(value)[:400] if value is not None else None,
                                                  "from": src}

# 运行时网格的 CellSize / LoadingRange（这是"单元格没被加载"最常见的根源）
runtime_hash = None
if wp is not None:
    runtime_hash, _ = probe(wp, ("runtime_hash", "RuntimeHash"))
if runtime_hash is not None:
    grids = None
    value, _ = probe(runtime_hash, ("grids", "Grids"))
    if value is not None:
        grids = value
    report["runtime_hash"] = {
        "class": runtime_hash.get_class().get_name(),
        "attrs_matching_keywords": list_attrs(runtime_hash, ("grid", "cell", "range")),
    }
    cell_info = []
    if grids:
        for grid in grids:
            entry = {}
            for label, candidates in (
                ("GridName", ("grid_name",)),
                ("CellSize", ("cell_size",)),
                ("LoadingRange", ("loading_range",)),
                ("bBlockOnSlowStreaming", ("b_block_on_slow_streaming",)),
            ):
                value, src = probe(grid, candidates)
                entry[label] = {"value": str(value) if value is not None else None, "from": src}
            cell_info.append(entry)
    report["runtime_grids"] = cell_info

# ---------------------------------------------------------------- actor 侧
actors = get_all_actors(world)
report["actor_count"] = len(actors)

spatially_loaded_true = 0
spatially_loaded_false = 0
spatially_loaded_unknown = 0
suspects = []

for actor in actors:
    cls = actor.get_class()
    cls_name = cls.get_name() if cls else "?"
    label = ""
    try:
        label = actor.get_actor_label()
    except Exception:
        pass

    loaded, loaded_src = probe(actor, SPATIALLY_LOADED_NAMES)
    editor_only, _ = probe(actor, EDITOR_ONLY_NAMES)
    grid, grid_src = probe(actor, RUNTIME_GRID_NAMES)
    folder = ""
    try:
        folder = str(actor.get_folder_path())
    except Exception:
        pass

    if loaded is None:
        spatially_loaded_unknown += 1
    elif loaded:
        spatially_loaded_true += 1
    else:
        spatially_loaded_false += 1

    # 只对"敌人/角色"这类放置出来会变灰的 actor 做详细输出
    interesting = any(k in cls_name for k in ("Enemy", "BowStance", "Warrior", "Character", "Pawn"))
    row = {
        "class": cls_name,
        "label": label,
        "spatially_loaded": loaded,
        "spatially_loaded_from": loaded_src,
        "editor_only": editor_only,
        "runtime_grid": str(grid) if grid is not None else None,
        "runtime_grid_from": grid_src,
        "folder": folder,
    }
    try:
        row["location"] = str(actor.get_actor_location())
    except Exception:
        pass
    if wp is not None:
        ok, is_in = try_call(wp, "is_actor_in_world", actor)
        if ok:
            row["world_partition_is_actor_in_world"] = is_in
    if interesting:
        suspects.append(row)

report["spatially_loaded_summary"] = {
    "true": spatially_loaded_true,
    "false": spatially_loaded_false,
    "unknown": spatially_loaded_unknown,
}
report["suspect_actors"] = suspects

# Data Layer：如果 actor 被分到未激活的层，也会表现为 Unloaded
wp_lib = getattr(unreal, "WorldPartitionBlueprintLibrary", None)
if wp_lib is not None:
    report["world_partition_blueprint_library"] = {
        "apis_matching_keywords": sorted(
            [n for n in dir(wp_lib) if any(k in n.lower() for k in ("layer", "stream", "load", "actor"))]
        )
    }

# ---------------------------------------------------------------- 落盘
(OUT_DIR / "diagnosis.json").write_text(
    json.dumps(report, ensure_ascii=False, indent=2, default=str), encoding="utf-8"
)

note("世界=%s actor 总数=%d" % (world_name, len(actors)))
note("spatially_loaded: true=%d false=%d unknown=%d"
     % (spatially_loaded_true, spatially_loaded_false, spatially_loaded_unknown))
note("可疑 actor（敌人/角色类）%d 个：" % len(suspects))
for row in suspects[:20]:
    note("  %s label=%s spatially_loaded=%s editor_only=%s grid=%s folder=%s"
         % (row["class"], row["label"], row["spatially_loaded"], row["editor_only"],
            row["runtime_grid"], row["folder"]))
note("完整结果: %s" % (OUT_DIR / "diagnosis.json"))
note("WP_DIAGNOSIS_OK")
