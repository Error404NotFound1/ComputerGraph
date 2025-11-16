#include "MapFbxExportSubsystem.h"

#include "Editor.h"
#include "FbxExporter.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "UnrealEdGlobals.h"
#include "Factories/FbxExportOption.h"
#include "Editor/UnrealEdEngine.h"

void UMapFbxExportSubsystem::ExportActiveWorld()
{
    if (!GEditor)
    {
        UE_LOG(LogTemp, Warning, TEXT("Editor 实例不存在，无法导出"));
        return;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("无可导出的世界"));
        return;
    }

    const FString MapName = World->GetOutermost()->GetName();
    const FString TargetPath = BuildExportPath(MapName);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetPath), true);

    UFbxExportOption* ExportOptions = NewObject<UFbxExportOption>();
    ExportOptions->LevelOfDetail = 0;
    ExportOptions->VertexColor = true;
    ExportOptions->bASCII = false;
    ExportOptions->Collision = false;

    UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
    Exporter->CreateDocument();
    Exporter->ExportLevelMesh(World, ExportOptions);
    Exporter->WriteToFile(*TargetPath);

    UE_LOG(LogTemp, Display, TEXT("地图 %s 已导出到 %s"), *MapName, *TargetPath);
}

FString UMapFbxExportSubsystem::BuildExportPath(const FString& MapName) const
{
    const FString BaseDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("MapExports"));
    return BaseDir / FString::Printf(TEXT("%s.fbx"), *MapName);
}

