// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

NIGHTFALLEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogNightfallEditor, Log, All);

class FNightfallEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
