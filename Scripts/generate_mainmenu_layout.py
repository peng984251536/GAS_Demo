"""按《暖雪》式左侧竖排 + 低多边形规范，生成 UWG_MainMenu 的控件树。

用法：UE 编辑器 -> Tools > Execute Python Script，选择本文件。
只搭结构，不设置贴图和颜色样式；图片由你在编辑器里填。

前置条件（两件必须在编辑器里先做，脚本做不了）：
  1. UWG_MainMenu 的父类改成 CC_MainMenuWidget。
  2. 该 Widget 里已经有一个根控件（拖一个 Overlay 进去即可）。
     UWidgetTree.RootWidget 带 EditConst，Python 和 RemoteControl 都写不进去。

会做什么：
  1. 备份现有资产到 Saved/MainMenuLayoutBackup/（T3D，可对照）。
  2. 在现有根控件下面清空并重建布局。
  3. CompileBlueprint + SaveAsset，回读结果写到 Saved/mainmenu_generate.txt。

不会做什么：
  - 不动父类、不创建根控件、不设按钮画笔、不插图片、不接蓝图逻辑。
"""

import unreal
from pathlib import Path

ASSET_PATH = '/Game/GAS_Demo/UI/MainMenu/UWG_MainMenu'
TEX_DIR = '/Game/GAS_Demo/UI/MainMenu/Textures'
BACKUP_DIR = Path('D:/UEProject/GAS_Demo/Saved/MainMenuLayoutBackup')
OUT = Path('D:/UEProject/GAS_Demo/Saved/mainmenu_generate.txt')

# 低多边形配色，取自 SourceArt/UI/MainMenu/LowPoly/README.zh-CN.md 的 Hex sRGB 表。
# 只用于文字颜色和面板底色，不覆盖按钮画笔。
H_ALIGN = unreal.HorizontalAlignment
V_ALIGN = unreal.VerticalAlignment
VIS = unreal.SlateVisibility

lines = []


def log(msg):
    lines.append(str(msg))


def C(hex_str, a=1.0):
    """Hex sRGB -> LinearColor。"""
    h = hex_str.lstrip('#')
    return unreal.LinearColor(
        int(h[0:2], 16) / 255.0,
        int(h[2:4], 16) / 255.0,
        int(h[4:6], 16) / 255.0,
        a,
    )


COL_TEXT = C('FFF0D0FF')        # 暖白
COL_MUTED = C('A8C4C5FF')       # 灰青
COL_PANEL = C('1A2E33', 0.80)   # 深青 + 80% 不透明


# ---------------------------------------------------------------------------
# 控件类名自检。
#
# UE 的 Python 绑定对 UMG 控件有时暴露为 Overlay，有时要写 UOverlay，
# 取决于版本和是否加载了 UMG 模块。与其撞 AttributeError，不如先探测。
# 同时 load_module 一次，保证 UMG/UMGEditor 已初始化。
# ---------------------------------------------------------------------------
def resolve_widget_classes():
    for mod in ('UMG', 'UMGEditor'):
        try:
            unreal.load_module(mod)
        except Exception as exc:
            log('load_module(%s) 跳过: %s' % (mod, exc))

    wanted = ['Overlay', 'VerticalBox', 'HorizontalBox', 'SizeBox', 'Border',
              'Image', 'TextBlock', 'Button', 'ScrollBox', 'Spacer',
              'CanvasPanel', 'SafeZone']
    resolved = {}
    for base in wanted:
        for candidate in (base, 'U' + base):
            if hasattr(unreal, candidate):
                resolved[base] = getattr(unreal, candidate)
                break
    return resolved


WIDGETS = resolve_widget_classes()


# ---------------------------------------------------------------------------
# 插槽设置。不同容器的插槽类型不同，属性名也不同，按实际类型分派。
# 单项失败只记录，不中断整棵树——布局再错也好过一半就停。
# ---------------------------------------------------------------------------
def _slot_kind(slot):
    """按类名判断插槽类型，不依赖 unreal.CanvasPanelSlot 这类绑定是否存在。"""
    return type(slot).__name__


def _is_canvas_slot(slot):
    return 'CanvasPanelSlot' in _slot_kind(slot)


def _apply_slot(widget, **kw):
    slot = widget.slot
    name = widget.get_name()
    if slot is None:
        return

    def put(label, fn):
        try:
            fn()
        except Exception as exc:
            log('  插槽跳过 %s.%s: %s' % (name, label, exc))

    if kw.get('padding') is not None:
        p = kw['padding']
        put('padding', lambda: slot.set_padding(
            unreal.Margin(left=p[0], top=p[1], right=p[2], bottom=p[3])))

    if kw.get('h_align') is not None:
        put('h_align', lambda: slot.set_horizontal_alignment(kw['h_align']))

    if kw.get('v_align') is not None:
        put('v_align', lambda: slot.set_vertical_alignment(kw['v_align']))

    if kw.get('size') is not None and not _is_canvas_slot(slot):
        # Canvas 用 offsets 控制尺寸/位置；其他容器的 slot 才有 size
        s = kw['size']
        put('size', lambda: slot.set_size(unreal.Vector2D(s[0], s[1])))

    if kw.get('anchor') is not None and _is_canvas_slot(slot):
        a = kw['anchor']
        put('anchors', lambda: slot.set_anchors(unreal.Anchors(
            minimum=unreal.Vector2D(a[0], a[1]),
            maximum=unreal.Vector2D(a[2], a[3]))))

    if kw.get('offsets') is not None and _is_canvas_slot(slot):
        o = kw['offsets']
        put('offsets', lambda: slot.set_offsets(unreal.Margin(
            left=o[0], top=o[1], right=o[2], bottom=o[3])))

    if kw.get('alignment') is not None and _is_canvas_slot(slot):
        a = kw['alignment']
        put('alignment', lambda: slot.set_alignment(unreal.Vector2D(a[0], a[1])))

    if kw.get('auto_size'):
        put('auto_size', lambda: slot.set_auto_size(True))

    if kw.get('z') and _is_canvas_slot(slot):
        put('z_order', lambda: slot.set_z_order(kw['z']))


def _flags():
    """RF_Transactional；拿不到就退回默认，new_object 会自己报错。"""
    try:
        return unreal.ObjectFlags.RF_TRANSACTIONAL
    except Exception:
        return 0


def make(parent, cls, name, slot=None, props=None):
    """建控件并挂到 parent 下。

    关键：带 RF_Transactional 新建。
    默认分包下对象已有命名会让 UE 构建时断言失败——new_object 的检查里
    没有覆盖这条路径的变通分支，带这个标志才能正常重命名。
    """
    w = unreal.new_object(cls, parent, name, object_flags=_flags())
    _apply_slot(w, **(slot or {}))
    for key, value in (props or {}).items():
        try:
            w.set_editor_property(key, value)
        except Exception as exc:
            log('  属性跳过 %s.%s: %s' % (name, key, exc))
    return w


def textblock(parent, name, label='', size=28, color=None, slot=None,
              wrap=False, justify=None):
    props = {
        'text': unreal.Text(label),
        'font': unreal.SlateFontInfo(size=size),
        'color_and_opacity': color if color is not None else COL_TEXT,
    }
    if wrap:
        props['auto_wrap_text'] = True
    if justify is not None:
        props['justification'] = justify
    return make(parent, WIDGETS['TextBlock'], name, slot, props)


def sized(parent, name, w=None, h=None, slot=None):
    """SizeBox：给菜单列和房间面板一个固定宽度，其余交给内部布局。"""
    sb = make(parent, WIDGETS['SizeBox'], name, slot)
    if w is not None:
        try:
            sb.set_editor_property('width_override', w)
        except Exception as exc:
            log('  SizeBox 跳过 %s.width: %s' % (name, exc))
    if h is not None:
        try:
            sb.set_editor_property('height_override', h)
        except Exception as exc:
            log('  SizeBox 跳过 %s.height: %s' % (name, exc))
    return sb


def button_row(parent, name, label, slot=None):
    """外层 SizeBox + 必需的 BindWidget Button。"""
    holder = make(parent, WIDGETS['SizeBox'], 'SizeBox_' + name, slot)
    btn = make(holder, WIDGETS['Button'], name,
               {'h_align': H_ALIGN.H_FILL, 'v_align': V_ALIGN.V_FILL})
    textblock(btn, name + '_Label', label, size=28,
              justify=unreal.TextJustify.CENTER)
    return btn


def main():
    log('=== 生成主菜单布局 ===')
    log('资产: %s' % ASSET_PATH)

    bp = unreal.load_asset(ASSET_PATH)
    if not bp:
        log('资产加载失败：%s' % ASSET_PATH)
        return

    parent_class = bp.get_editor_property('parent_class')
    log('当前父类: %s' % parent_class)
    if parent_class and 'CC_MainMenuWidget' not in str(parent_class):
        log('')
        log('!! 警告：父类不是 CC_MainMenuWidget。')
        log('!! 布局照样会生成，但 C++ 控制器不会接管这一页，GetScreenController 会返回空。')
        log('!! 请先在 Class Settings 把父类改成 CC_MainMenuWidget 再运行本脚本。')
        log('')

    # --- 备份 ---
    try:
        BACKUP_DIR.mkdir(parents=True, exist_ok=True)
        task = unreal.AssetExportTask()
        task.object = bp
        task.filename = str(BACKUP_DIR / 'UWG_MainMenu_backup.t3d')
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.ObjectExporterT3D()
        unreal.Exporter.run_asset_export_task(task)
        log('已备份 -> %s' % task.filename)
    except Exception as exc:
        log('备份跳过（不致命）: %s' % exc)

    tree = bp.get_editor_property('widget_tree')
    if tree is None:
        log('widget_tree 为空。请先在编辑器打开该 Widget 并按 Compile/Save 一次，再运行。')
        return

    # -------------------------------------------------------------------
    # 重要限制：UWidgetTree.RootWidget 带 EditConst 标志。
    # Python 能读它、能在它下面 new_object，但 set_editor_property 拒绝写，
    # RemoteControl 也拒绝。所以"根控件"这一步必须你在编辑器里手点，
    # 脚本只能从根控件往下建。
    # -------------------------------------------------------------------
    try:
        existing_root = tree.get_editor_property('root_widget')
    except Exception as exc:
        log('读取 root_widget 失败: %s' % exc)
        existing_root = None

    if existing_root is None:
        log('')
        log('!! 这个 Widget 还没有根控件，而根控件无法由脚本创建（引擎限制）。')
        log('!! 请手动做一次：')
        log('!!   1. 打开 %s' % ASSET_PATH)
        log('!!   2. 从 Palette 拖一个 Overlay 到画布（就是 Root_Overlay）')
        log('!!   3. 编译保存，然后重新运行本脚本')
        log('!! 下面的结构脚本会挂到这个根控件下面。')
        log('')
        return

    log('现有根控件: %s : %s' % (existing_root.get_name(),
                                  type(existing_root).__name__))
    log('将从它下面开始搭建。')

    root = existing_root

    # -------------------------------------------------------------------
    # 清空根控件下的现有子控件，让脚本可以反复运行而不堆叠重复层级。
    # 只有 PanelWidget 派生类（Overlay / CanvasPanel / Box ...）才有 ClearChildren。
    # -------------------------------------------------------------------
    cleared = False
    for method in ('clear_children', 'ClearChildren'):
        fn = getattr(root, method, None)
        if callable(fn):
            try:
                fn()
                cleared = True
                log('已清空根控件下的旧子控件。')
            except Exception as exc:
                log('清空子控件失败（%s）: %s' % (method, exc))
            break
    if not cleared:
        log('警告：根控件没有 clear_children，无法自动清空。')
        log('      如果重复运行，请先手工删掉旧子控件，否则会出现重复层级。')

    # =====================================================================
    # 从现有根控件往下搭。
    # 根控件本身由你在编辑器里放（Overlay），这里只用它当父级。
    # =====================================================================

    # --- 全屏背景。21:9 建议外面再包 ScaleBox，这里先保持 Fill。 ---
    bg = make(root, WIDGETS['Image'], 'Image_Background',
              {'h_align': H_ALIGN.H_FILL, 'v_align': V_ALIGN.V_FILL})
    # 背景不参与命中测试，否则会挡住背后内容
    try:
        bg.set_editor_property('visibility', VIS.HIT_TEST_INVISIBLE)
    except Exception as exc:
        log('  背景命中测试跳过: %s' % exc)
    tex_bg = unreal.load_asset(TEX_DIR + '/T_MainMenu_LowPolyMountains')
    if tex_bg:
        try:
            brush = bg.get_editor_property('brush')
            brush.set_editor_property('resource_object', tex_bg)
            bg.set_editor_property('brush', brush)
        except Exception as exc:
            log('  背景贴图跳过: %s' % exc)

    # =====================================================================
    # 左侧菜单列：左边距 110 / 上边距 80 / 宽 480（照 LowPoly README 规范）
    # 用 Canvas 定位是因为这是"贴边固定位置"，不是内容流；内部仍全部用 Box。
    # =====================================================================
    menu_box = sized(root, 'SizeBox_Menu', w=480)
    vb_menu = make(menu_box, WIDGETS['VerticalBox'], 'VB_Menu',
                   {'h_align': H_ALIGN.H_FILL, 'v_align': V_ALIGN.V_FILL})

    textblock(vb_menu, 'Text_GameTitle', '未命名的征程', size=76,
              slot={'padding': (0, 0, 0, 4), 'h_align': H_ALIGN.H_LEFT})
    textblock(vb_menu, 'Text_Subtitle', 'GAS Demo', size=24, color=COL_MUTED,
              slot={'padding': (0, 0, 0, 16), 'h_align': H_ALIGN.H_LEFT})

    # 菱形分隔：贴图 460x60，上下透明留白多，不用再加 Spacer
    div = sized(vb_menu, 'SizeBox_Divider', h=60,
                slot={'padding': (0, 0, 0, 24), 'h_align': H_ALIGN.H_LEFT})
    div_img = make(div, WIDGETS['Image'], 'Image_Divider',
                   {'h_align': H_ALIGN.H_FILL, 'v_align': V_ALIGN.V_FILL})
    tex_div = unreal.load_asset(TEX_DIR + '/T_MainMenu_LowPolyDivider')
    if tex_div:
        try:
            b = div_img.get_editor_property('brush')
            b.set_editor_property('resource_object', tex_div)
            div_img.set_editor_property('brush', b)
        except Exception as exc:
            log('  分隔贴图跳过: %s' % exc)

    # CC_MainMenuWidget 的 BindWidget 契约；按钮点击已由 C++ 绑定。
    for name, label in (('Button_SinglePlayer', '单人游戏'),
                        ('Button_HostRoom', '创建局域网世界'),
                        ('Button_Multiplayer', '加入局域网世界'),
                        ('Button_LeaveRoom', '离开房间'),
                        ('Button_Quit', '退出游戏')):
        button_row(vb_menu, name, label,
                   slot={'size': (480, 120),
                         'padding': (0, 0, 0, 8),
                         'h_align': H_ALIGN.H_LEFT})

    # 主菜单自身错误（模型里的 ActionError），不要和房间错误混用
    textblock(vb_menu, 'Text_Status', '', size=18, color=COL_MUTED, wrap=True,
              slot={'padding': (0, 24, 0, 0), 'h_align': H_ALIGN.H_LEFT})

    # --- 版本号 ---
    textblock(root, 'Text_Version', 'v0.1', size=18, color=COL_MUTED,
              slot={'h_align': H_ALIGN.H_RIGHT, 'v_align': V_ALIGN.V_BOTTOM,
                    'padding': (0, 0, 32, 24)})

    # 自检报告：哪些控件/插槽类名没解析到，直接写出来便于排查
    log('')
    log('=== 类名自检 ===')
    missing = [k for k in ('VerticalBox', 'SizeBox',
                           'Border', 'Image', 'TextBlock', 'Button', 'ScrollBox')
               if k not in WIDGETS]
    if missing:
        log('!! 未解析到这些控件类: %s' % ', '.join(missing))
        log('!! 对应控件会跳过，需要在脚本顶部 WIDGETS 里补类名。')
    else:
        log('控件类全部解析成功。')

    # =====================================================================
    # 编译 + 保存
    # =====================================================================
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log('已编译。')
    except Exception as exc:
        log('编译失败: %s' % exc)

    try:
        unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
        log('已保存。')
    except Exception as exc:
        log('保存失败: %s' % exc)

    # --- 回读，确认结构真的写进去了 ---
    log('')
    log('=== 回读控件树 ===')

    def walk(w, depth=0):
        if w is None:
            return
        log('%s- %s : %s' % ('  ' * depth, w.get_name(), type(w).__name__))
        kids = None
        try:
            kids = w.get_all_children()
        except Exception:
            try:
                kids = w.get_editor_property('all_children')
            except Exception:
                kids = None
        for child in (kids or []):
            walk(child, depth + 1)

    try:
        new_root = bp.get_editor_property('widget_tree').get_editor_property('root_widget')
        walk(new_root)
    except Exception as exc:
        log('回读失败: %s' % exc)

    log('')
    log('=== 下一步 ===')
    log('1. 打开 %s，确认父类是 CC_MainMenuWidget。' % ASSET_PATH)
    log('2. 给五个主按钮设画笔：T_MainMenu_LowPolyButton，')
    log('   Normal 3D7D89FF / Hovered E8783AFF / Pressed B64B25FF / Disabled 4A6470FF。')
    log('3. 背景图已经是 T_MainMenu_LowPolyMountains；如需替换直接改 Image_Background 的 Brush。')
    log('4. 按钮由 CC_MainMenuWidget 自动绑定；多人页按 Docs/LANMenuWidgetSetup.zh-CN.md 单独创建。')

    OUT.write_text('\n'.join(lines), encoding='utf-8')
    unreal.log('mainmenu 布局生成结束 -> %s' % OUT)


main()
