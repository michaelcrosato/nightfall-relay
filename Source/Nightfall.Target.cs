// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class NightfallTarget : TargetRules
{
	public NightfallTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;

		ExtraModuleNames.Add("Nightfall");
	}
}
