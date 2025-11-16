#pragma once

#include "Subsystems/EditorSubsystem.h"

#include "MapFbxExportSubsystem.generated.h"

UCLASS()
class UMapFbxExportSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(CallInEditor, Category = "Map FBX Exporter")
    void ExportActiveWorld();

private:
    FString BuildExportPath(const FString& MapName) const;
};

