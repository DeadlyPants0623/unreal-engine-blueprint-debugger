using UnrealBuildTool;

public class BPExecFlowViewer : ModuleRules
{
	public BPExecFlowViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"BlueprintGraph",
			"GraphEditor",
			"Kismet",
			"AssetRegistry",
			"Slate",
			"SlateCore",
			"InputCore",
			"ApplicationCore",
			"ToolMenus",
			"Projects",
		});
	}
}


