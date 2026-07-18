using UnrealBuildTool;

public class ShanHaiMirror : ModuleRules
{
	public ShanHaiMirror(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"GameplayTags"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore"
		});

		// 模块根目录加入私有 include path，使得 Framework/SHMCoreTypes.h 等跨目录引用可用
		PrivateIncludePaths.Add(ModuleDirectory);
	}
}
