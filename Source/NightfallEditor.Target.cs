// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class NightfallEditorTarget : TargetRules
{
	public NightfallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;

		ExtraModuleNames.Add("Nightfall");
		ExtraModuleNames.Add("NightfallEditor");
	}
}
