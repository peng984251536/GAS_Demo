# GAS_Demo

基于 Unreal Engine 5.6 的 C++ Gameplay Ability System 示例项目。

## 仓库内容

保留 C++ 源码、项目配置、文档、测试和辅助工具源码。

本仓库不包含 `Content/` 与 `SourceArt/`。蓝图、地图、角色、动画、贴图等资源保留在原电脑的项目目录中，不上传 GitHub。编译缓存、IDE 本地状态和工具编译产物同样不上传。

## 使用说明

1. 安装 Unreal Engine 5.6 和对应 C++ 开发工具。
2. 将完整项目的 `Content/` 和需要的 `SourceArt/` 资源另外复制到项目目录。
3. 为 `GAS_Demo.uproject` 生成项目文件，编译后打开项目。

缺少这些资源时，克隆本仓库不能直接恢复或运行完整游戏。

## 后续上传

在项目根目录执行：

```powershell
git add .
git commit -m "Describe your changes"
git push
```

忽略规则会自动跳过资源和缓存。当前仓库保留 Git LFS 配置，但被忽略的资源不会参与上传。
