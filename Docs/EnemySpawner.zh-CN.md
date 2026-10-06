# 场景怪物生成点

类：`CC_EnemySpawner`。头文件和实现分别放在 `Public/Character/Spawner`、`Private/Character/Spawner`。

1. 编译项目后，在内容浏览器的 C++ Classes 中找到 `CC_EnemySpawner`，拖进关卡；也可以先创建它的蓝图子类。
2. 在 Details → Spawner 设置 `Enemy Class`，例如 `BP_CC_Enemy02` 或 `BP_Enemy_Warrior`。
3. 设置 `Spawn Count`（默认 5）和 `Spawn Radius`（默认 500 厘米），将生成点放在地面附近。红色球形线框辅助显示半径，实际候选位置分布在水平圆内。
4. 开始游戏后默认生成一次。若需要事件触发，关闭 `Spawn On Begin Play`，在服务器蓝图调用 `Spawn Enemies`，返回值是本次实际生成数量。

每次手动调用都会额外生成一批，不自动补怪、不追踪存活数量。生成器使用怪物蓝图原本的配置和 AIController，支持原 Boris 及新增 Polyart 敌人。

生成位置会搜索具有 WorldStatic 碰撞的地面，跳过陡坡和碰撞位置。空间不足时不会强行挤出指定数量，日志会提示实际生成数；可增大半径或调整位置。多层建筑会命中搜索范围内最上层表面，可缩小 `Ground Search Distance` 限定目标楼层。地面移动平台不在当前搜索范围内。

联机只由服务器生成；怪物自身仍需开启复制。AI 寻路沿用场景的 NavMesh 配置，本生成器不创建导航数据，也不保证候选点位于可达导航区域。
