using UnrealBuildTool;
using System.Collections.Generic;

public class HearthEditorTarget : TargetRules
{
	public HearthEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Hearth");
	}
}
