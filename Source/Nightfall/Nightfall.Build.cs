// Copyright Nightfall Relay. All Rights Reserved.

using UnrealBuildTool;

public class Nightfall : ModuleRules
{
	public Nightfall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"ModularGameplay",
			"GameFeatures",
			"PhysicsCore",
			"DeveloperSettings",
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"RenderCore",
			"RHI",
			"Projects",
			"Json",
			"JsonUtilities",
		});
	}
}
