using UnrealBuildTool;

public class MapFbxExporter : ModuleRules
{
    public MapFbxExporter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "EditorFramework",
            "LevelEditor",
            "UnrealEd",
            "Slate",
            "SlateCore"
        });
    }
}

