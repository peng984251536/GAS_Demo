"""UE 5.6 Python：给全局音效配置补上默认的播放规则（可重复运行，只补缺省值，不覆盖你手动改过的设置）。

用法：工具 → 执行 Python 脚本，或 Output Log（Cmd 模式）输入
    py D:/UEProject/GAS_Demo/Scripts/setup_audio_rules.py

会做的事：
- 创建 /Game/GAS_Demo/Audio/Rules/ATT_SFX_Default（3D 衰减：内半径 200，衰减距离 2500），设为配置的 Default Attenuation；
- 创建 SC_Footstep（同时最多 8 个）、SC_Hit（同时最多 6 个），挂到 Footstep.* / Hit.* 事件；
- 为 Footstep.* / Attack.* / Hit.* 设置轻微的随机音高与音量（仍为默认 1~1 的才会改）。
"""
import unreal

ROOT = "/Game/GAS_Demo/Audio/Rules"
CONFIG_PATH = "/Game/GAS_Demo/Audio/DA_AudioConfig_Default"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets.make_directory(ROOT)

config = assets.load_asset(CONFIG_PATH)
assert isinstance(config, unreal.CC_AudioConfig), CONFIG_PATH


def ensure(name, asset_class, factory):
    path = ROOT + "/" + name
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    return tools.create_asset(name, ROOT, asset_class, factory), True


attenuation, created = ensure("ATT_SFX_Default", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
if created:
    s = attenuation.get_editor_property("attenuation")
    s.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    s.set_editor_property("attenuation_shape_extents", unreal.Vector(200.0, 0.0, 0.0))
    s.set_editor_property("falloff_distance", 2500.0)
    attenuation.set_editor_property("attenuation", s)
    assets.save_loaded_asset(attenuation)


def make_concurrency(name, max_count):
    asset, new = ensure(name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    if new:
        c = asset.get_editor_property("concurrency")
        c.set_editor_property("max_count", max_count)
        c.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_FARTHEST_THEN_OLDEST)
        asset.set_editor_property("concurrency", c)
        assets.save_loaded_asset(asset)
    return asset


rules = {  # 前缀: (并发, 音量范围, 音高范围)
    "Footstep.": (make_concurrency("SC_Footstep", 8), (0.85, 1.0), (0.95, 1.05)),
    "Hit.": (make_concurrency("SC_Hit", 6), (0.9, 1.0), (0.95, 1.05)),
    "Attack.": (None, (0.9, 1.0), (0.96, 1.04)),
}

if not config.get_editor_property("default_attenuation"):
    config.set_editor_property("default_attenuation", attenuation)

entries = dict(config.get_editor_property("events"))
changed = []
for key, entry in entries.items():
    name = str(key)
    for prefix, (concurrency, vol, pitch) in rules.items():
        if not name.startswith(prefix):
            continue
        if concurrency and not entry.get_editor_property("concurrency"):
            entry.set_editor_property("concurrency", concurrency)
        for field, (lo, hi) in (("volume_range", vol), ("pitch_range", pitch)):
            r = entry.get_editor_property(field)
            if abs(r.get_editor_property("min") - 1.0) < 1e-4 and abs(r.get_editor_property("max") - 1.0) < 1e-4:
                r.set_editor_property("min", lo)
                r.set_editor_property("max", hi)
                entry.set_editor_property(field, r)
        entries[key] = entry
        changed.append(name)
config.set_editor_property("events", entries)
assert assets.save_loaded_asset(config, only_if_is_dirty=False)
unreal.log("AUDIO_RULES_OK " + ", ".join(sorted(changed)))
