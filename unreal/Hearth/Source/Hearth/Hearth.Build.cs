using UnrealBuildTool;

public class Hearth : ModuleRules
{
	public Hearth(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore",
			"AIModule", "NavigationSystem",
			"WebSockets", "Json", "JsonUtilities"
		});
	}
}
