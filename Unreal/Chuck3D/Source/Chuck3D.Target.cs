using UnrealBuildTool;
public class Chuck3DTarget : TargetRules
{
    public Chuck3DTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Chuck3D");
    }
}
