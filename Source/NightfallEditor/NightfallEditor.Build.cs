// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class NightfallEditor : ModuleRules
{
	public NightfallEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Nightfall",
			"GameFeatures",
			"PCG",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"Projects",
		});
	}
}
