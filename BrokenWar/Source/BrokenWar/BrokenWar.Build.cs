// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BrokenWar : ModuleRules
{
	public BrokenWar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"BrokenWar",
			"BrokenWar/Variant_Horror",
			"BrokenWar/Variant_Horror/UI",
			"BrokenWar/Variant_Shooter",
			"BrokenWar/Variant_Shooter/AI",
			"BrokenWar/Variant_Shooter/UI",
			"BrokenWar/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
