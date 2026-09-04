using UnrealBuildTool;
using System.Collections.Generic;

public class HearthTarget : TargetRules
{
	public HearthTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Hearth");
	}
}
