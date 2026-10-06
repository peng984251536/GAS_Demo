// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GAS_Demo : ModuleRules
{
	public GAS_Demo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AnimGraphRuntime",
			"GameplayTags",
			"MotionWarping",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			// Lyra 风格分层 UI：直接使用官方 CommonUI 容器与输入路由。
			"CommonUI",
			"CommonInput",
			"Slate",
			"SlateCore",
			"GameplayAbilities",
			"GameplayTasks",
			"ActorSequence",
			"LevelSequence",
			"MovieScene",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"AssetRegistry",
			// OnlineRoom 通过经典 OnlineSubsystem 的 Session 接口实现房间发现与加入。
			// OnlineSubsystemUtils 负责按 World 获取正确的 PIE 子系统实例。
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		// 普通 UObject 子对象在启用 Iris 的构建中注册复制片段。
		SetupIrisSupport(Target);

		PublicIncludePaths.AddRange(new string[] {
			"GAS_Demo",
			"GAS_Demo/Variant_Strategy",
			"GAS_Demo/Variant_Strategy/UI",
			"GAS_Demo/Variant_TwinStick",
			"GAS_Demo/Variant_TwinStick/AI",
			"GAS_Demo/Variant_TwinStick/Gameplay",
			"GAS_Demo/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
