using UnrealBuildTool;

public class BPHealthAnalyzer : ModuleRules
{
	public BPHealthAnalyzer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"AssetRegistry",
			"BlueprintGraph",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Kismet",
			"Projects",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd"
		});
	}
}
