#include "MapFbxExporterModule.h"

#include "LevelEditor.h"
#include "MapFbxExportSubsystem.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FMapFbxExporterModule"

void FMapFbxExporterModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMapFbxExporterModule::RegisterMenus));
}

void FMapFbxExporterModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
}

void FMapFbxExporterModule::RegisterMenus()
{
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.File"))
    {
        FToolMenuSection& Section = Menu->AddSection("MapFbxExporter", LOCTEXT("MapExportSection", "Map FBX 导出"));
        Section.AddMenuEntry(
            "ExportActiveWorldAsFbx",
            LOCTEXT("ExportTitle", "导出当前地图为 FBX"),
            LOCTEXT("ExportTooltip", "将当前打开的地图合并并导出为 FBX，用于外部 OpenGL 查看。"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([]()
            {
                if (UMapFbxExportSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMapFbxExportSubsystem>())
                {
                    Subsystem->ExportActiveWorld();
                }
            }))
        );
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMapFbxExporterModule, MapFbxExporter)

