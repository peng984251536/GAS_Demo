"""Run with UE 5.6 PythonScript commandlet after building GAS_DemoEditor."""
import json
import pathlib
import shutil
import unreal

ROOT = "/Game/GAS_Demo/GameplayEffects/NewBuff"
DISK = pathlib.Path(unreal.Paths.project_content_dir()) / "GAS_Demo/GameplayEffects/NewBuff"
BACKUP = pathlib.Path(unreal.Paths.project_saved_dir()) / "BuffSystem/NewOffersBefore"
BACKUP.mkdir(parents=True, exist_ok=True)
pool_file = DISK / "DA_BuffOfferPool_New.uasset"
if pool_file.exists() and not (BACKUP / pool_file.name).exists():
    shutil.copy2(pool_file, BACKUP / pool_file.name)
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
specs = [
    ("GE_Buff_AttackPermanent", "CC_GE_AttackPermanent"),
    ("GE_Buff_RegenPercent", "CC_GE_RegenPercent"),
    ("GE_Buff_SuperArmor", "CC_GE_ArmorTen"),
    ("GE_Buff_CharmAttack", "CC_GE_CharmAttackTen"),
    ("GE_Buff_SplitArrow", "CC_GE_SplitPermanent"),
    ("GE_Buff_DoubleArrow", "CC_GE_DoubleArrowPermanent"),
    ("GE_Buff_DeflectArrow", "CC_GE_DeflectArrowPermanent"),
]
classes = {}
for name, parent_name in specs:
    path = ROOT + "/" + name
    parent = unreal.load_class(None, "/Script/GAS_Demo." + parent_name)
    assert parent, parent_name
    if assets.does_asset_exist(path):
        bp = assets.load_asset(path)
        assert unreal.MathLibrary.class_is_child_of(bp.generated_class(), parent), path
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = tools.create_asset(name, ROOT, unreal.Blueprint, factory)
        assert bp, path
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, only_if_is_dirty=False), path
    classes[name] = bp.generated_class()

summon = unreal.load_class(None, ROOT + "/GE_Buff_SummonAlly.GE_Buff_SummonAlly_C")
assert summon, "Existing summon asset is required."
ordered = [classes[specs[i][0]] for i in range(4)] + [summon] + [classes[specs[i][0]] for i in (4, 5, 6)]
pool = assets.load_asset(ROOT + "/DA_BuffOfferPool_New")
assert pool and pool.get_class().get_name() == "CC_BuffOfferPool"
entries = list(pool.get_editor_property("entries"))
for cls in ordered:
    cdo = unreal.get_default_object(cls)
    components = cdo.get_editor_property("ge_components")
    ui = next((c for c in components if c.get_class().get_name() == "CC_BuffUIData"), None)
    assert ui and str(ui.get_editor_property("display_name")), cls.get_path_name()
    # 保留现有选项的顺序与等级，只追加尚未存在的选项。
    if any(entry.get_editor_property("effect") == cls for entry in entries):
        continue
    option = unreal.CC_BuffOfferOption()
    option.set_editor_property("effect", cls)
    option.set_editor_property("level", 1.0)
    entries.append(option)
# 保留这次加入反弹前的池，首次历史备份仍保留在 NewOffersBefore。
deflect_backup = BACKUP.parent / "BeforeDeflectPool.uasset"
if pool_file.exists() and not deflect_backup.exists():
    shutil.copy2(pool_file, deflect_backup)
pool.set_editor_property("entries", entries)
pool.set_editor_property("offer_count", 3)
assert assets.save_loaded_asset(pool, only_if_is_dirty=False)
report = {"pool": pool.get_path_name(), "offer_count": 3,
          "effects": [entry.get_editor_property("effect").get_path_name() for entry in entries]}
(BACKUP.parent / "NewOffersManifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("NEW_BUFF_OFFERS_OK " + json.dumps(report, ensure_ascii=False))
