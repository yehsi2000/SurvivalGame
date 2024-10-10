using UnrealBuildTool;

public class ModuleTest: ModuleRules
{
    public ModuleTest(ReadOnlyTargetRules Target) : base(Target){
        PrivateDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine"});
    }
}