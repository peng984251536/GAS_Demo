"""检查 UWG_MainMenu 当前状态：父类、控件树、已有控件名。

用法：在 UE 编辑器里执行（Tools > Execute Python Script，或 Output Log 里跑 python 命令）。
结果写到 Saved/mainmenu_inspect.txt，不修改任何资产。

本轮不猜布局，先把现状读出来，避免按过时文档（WBP_MainMenu / MVVM / RLMainMenuWidget）
去改一个其实已经换过框架的资产。
"""

import unreal
from pathlib import Path

ASSET_PATH = '/Game/GAS_Demo/UI/MainMenu/UWG_MainMenu'
OUT = Path('D:/UEProject/GAS_Demo/Saved/mainmenu_inspect.txt')

lines = []


def log(msg):
    lines.append(str(msg))


def safe(label, fn):
    """遍历资产时任何一步失败都不该中断整份报告。"""
    try:
        return fn()
    except Exception as exc:
        log('%s -> 失败: %s' % (label, exc))
        return None


log('=== 资产 ===')
log('路径: %s' % ASSET_PATH)

bp = safe('load_asset', lambda: unreal.load_asset(ASSET_PATH))
if not bp:
    log('资产加载失败，确认路径与是否已编译 C++。')
    OUT.write_text('\n'.join(lines), encoding='utf-8')
    raise SystemExit

log('类型: %s' % type(bp).__name__)
log('资产路径: %s' % safe('get_path_name', lambda: bp.get_path_name()))

# 父类决定了这个 Widget 归哪套框架管。现在应当是 CC_MainMenuWidget，
# 如果还停在 RLMainMenuWidget 说明父类没改，蓝图接线会整条失效。
log('')
log('=== 父类 ===')
parent = safe('get_parent_class', lambda: bp.get_editor_property('parent_class'))
log('parent_class: %s' % parent)
if parent:
    log('parent_class 完整名: %s' % safe('parent path', lambda: parent.get_path_name()))

# WidgetTree 是这个资产的设计器内容。为空 = 还没搭过布局。
log('')
log('=== 控件树 ===')
tree = safe('get widget_tree', lambda: bp.get_editor_property('widget_tree'))
log('widget_tree: %s' % tree)

root = None
if tree:
    root = safe('get root_widget', lambda: tree.get_editor_property('root_widget'))
    log('root_widget: %s' % root)

# 递归打印控件层级，同时收集名字，方便和 C++ 要求的 BindWidget 名对齐。
log('')
log('=== 层级 ===')


def walk(widget, depth=0):
    if widget is None:
        return
    indent = '  ' * depth
    name = safe('name', lambda: widget.get_name())
    cls = type(widget).__name__
    try:
        slot = widget.slot
    except Exception:
        slot = None
    visibility = safe('visibility', lambda: widget.get_editor_property('visibility'))
    log('%s- %s : %s%s' % (indent, name, cls,
                           '' if visibility is None else ' [%s]' % visibility))
    try:
        children = widget.get_editor_property('all_children')
    except Exception:
        children = None
    if not children:
        children = safe('get_all_children', lambda: widget.get_all_children())
    for child in (children or []):
        walk(child, depth + 1)


if root:
    walk(root)
else:
    log('（没有根控件——说明这个资产目前是空的，需要从零搭布局。）')

# BindWidget 要求精确的控件名和类型，单独列出来便于核对。
log('')
log('=== 所有控件名与类型 ===')
names = safe('collect names', lambda: [
    (w.get_name(), type(w).__name__)
    for w in (tree.get_all_widgets() if tree and hasattr(tree, 'get_all_widgets') else [])
])
if names:
    for name, cls in names:
        log('%s : %s' % (name, cls))
else:
    log('（未取到，或资产为空。）')

OUT.write_text('\n'.join(lines), encoding='utf-8')
unreal.log('mainmenu 检查完成 -> %s' % OUT)
