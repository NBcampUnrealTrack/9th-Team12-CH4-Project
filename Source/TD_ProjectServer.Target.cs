using UnrealBuildTool;
using System.Collections.Generic;

public class TD_ProjectServerTarget : TargetRules
{
    public TD_ProjectServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        bUseLoggingInShipping = true;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("TD_Project");
    }
}