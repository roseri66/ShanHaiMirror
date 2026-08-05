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
			"GameplayTags",
			// LLM 层（第四次开工）：HTTP 调用 + JSON 解析
			"HTTP",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore"
		});

		// 模块根目录加入私有 include path，使得 Framework/SHMCoreTypes.h 等跨目录引用可用
		PrivateIncludePaths.Add(ModuleDirectory);

		// ====================================================================
		// SHM_DEV_DIRECT_LLM —— 客户端直连 LLM 的开关（D-23，默认关闭）
		//
		// 关闭时 FSHMLlmProvider 与 FSHMPromptBuilder 整体不编译，
		// 降级链是 Remote → Local。
		//
		// **为什么必须默认关闭**：prompt 搬到服务端之后，SHMPromptBuilder.cpp
		// 就成了第二份 prompt 真源。两份同时生效 = 同一个游戏两套人格，
		// 而且会随各自的修改越漂越远。D-23 把它列为红线：
		// 「双写是会持续流血的伤口，不是能欠的债」。
		//
		// 什么时候打开：没有后端可用、又想验证 LLM 相关改动时，
		// 改成 =1 重新编译。**打开后 prompt 走的是客户端那份，
		// 与服务端的 prompt.yaml 允许漂移、不保证一致**——这是明说的，
		// 不是遗漏。
		// ====================================================================
		PublicDefinitions.Add("SHM_DEV_DIRECT_LLM=0");
	}
}
