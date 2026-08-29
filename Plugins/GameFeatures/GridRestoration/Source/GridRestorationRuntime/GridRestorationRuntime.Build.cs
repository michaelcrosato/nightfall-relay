// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class GridRestorationRuntime : ModuleRules
{
	public GridRestorationRuntime(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
			// The only dependency on the game itself: this plugin reads core entity
			// classes and services, and the core knows nothing about this plugin.
			"Nightfall",
		});
	}
}
