using UnrealBuildTool;
public class Chuck3DEditorTarget : TargetRules
{
    public Chuck3DEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Chuck3D");
    }
}
