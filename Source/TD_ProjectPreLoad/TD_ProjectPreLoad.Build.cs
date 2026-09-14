using UnrealBuildTool;

public class TD_ProjectPreLoad : ModuleRules
{
    public TD_ProjectPreLoad(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "PreLoadScreen",
            "Slate",
            "SlateCore"
        });
    }
}
