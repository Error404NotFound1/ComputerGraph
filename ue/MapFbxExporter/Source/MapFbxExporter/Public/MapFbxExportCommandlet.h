#pragma once

#include "Commandlets/Commandlet.h"

#include "MapFbxExportCommandlet.generated.h"

UCLASS()
class UMapFbxExportCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UMapFbxExportCommandlet();
    virtual int32 Main(const FString& Params) override;
};

