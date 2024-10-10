// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class SurvivalGameServerTarget : TargetRules
{
	public SurvivalGameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V2;

		bUsesSteam = true;
		bUseLoggingInShipping = true;

		GlobalDefinitions.Add("UE4_PROJECT_STEAMPRODUCTNAME=\"Spacewar\""); //testing name
        GlobalDefinitions.Add("UE4_PROJECT_STEAMGAMEDESC=\"SurvivalGame\"");
        GlobalDefinitions.Add("UE4_PROJECT_STEAMGAMEDIR=\"Spacewar\"");
        GlobalDefinitions.Add("UE4_PROJECT_STEAMSHIPPINGID=480"); //setting test id

        ExtraModuleNames.AddRange( new string[] { "SurvivalGame" } );
	}
}
