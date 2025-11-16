#include "MapFbxExportCommandlet.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "FbxExporter.h"
#include "Factories/FbxExportOption.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

UMapFbxExportCommandlet::UMapFbxExportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UMapFbxExportCommandlet::Main(const FString& Params)
{
    FString MapPath;
    FString OutputDir;
    FParse::Value(*Params, TEXT("-Map="), MapPath);
    FParse::Value(*Params, TEXT("-Out="), OutputDir);

    if (MapPath.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("请通过 -Map=/Game/MyMap 指定需要导出的地图"));
        return 1;
    }

    if (OutputDir.IsEmpty())
    {
        OutputDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CommandletExports"));
    }

    IFileManager::Get().MakeDirectory(*OutputDir, true);

    if (!FPackageName::DoesPackageExist(MapPath))
    {
        UE_LOG(LogTemp, Error, TEXT("地图 %s 不存在"), *MapPath);
        return 1;
    }

    if (!FEditorFileUtils::LoadMap(MapPath, false, true))
    {
        UE_LOG(LogTemp, Error, TEXT("地图 %s 加载失败"), *MapPath);
        return 1;
    }

    if (!GEditor)
    {
        UE_LOG(LogTemp, Error, TEXT("GEditor 不可用"));
        return 1;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("未找到世界"));
        return 1;
    }

    const FString MapName = World->GetOutermost()->GetName();
    const FString TargetPath = OutputDir / (MapName + TEXT(".fbx"));

    UFbxExportOption* ExportOptions = NewObject<UFbxExportOption>();
    ExportOptions->bASCII = false;
    ExportOptions->LevelOfDetail = 0;

    UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
    Exporter->CreateDocument();
    Exporter->ExportLevelMesh(World, ExportOptions);
    Exporter->WriteToFile(*TargetPath);

    UE_LOG(LogTemp, Display, TEXT("地图 %s 已导出到 %s"), *MapName, *TargetPath);
    return 0;
}

