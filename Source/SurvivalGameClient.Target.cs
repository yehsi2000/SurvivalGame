// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class SurvivalGameClientTarget : TargetRules
{
	public SurvivalGameClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;

        GlobalDefinitions.Add("UE4_PROJECT_STEAMGAMEDIR=\"Spacewar\"");
        GlobalDefinitions.Add("UE4_PROJECT_STEAMSHIPPINGID=480"); //setting test id

        ExtraModuleNames.AddRange( new string[] { "SurvivalGame" } );
	}
}
