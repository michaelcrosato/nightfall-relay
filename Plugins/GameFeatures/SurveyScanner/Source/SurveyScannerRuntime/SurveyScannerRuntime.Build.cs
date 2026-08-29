// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class SurveyScannerRuntime : ModuleRules
{
	public SurveyScannerRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ModularGameplay",
			"GameFeatures",
			"EnhancedInput",
			"Slate",
			"SlateCore",
			"Nightfall",
		});
	}
}
