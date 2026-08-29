// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Nightfall.h"
#include "Stats/Stats.h"

SURVEYSCANNERRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogSurveyScanner, Log, All);

/** Reported in the core performance HUD's per-system list. */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Survey Scanner"), STAT_Nightfall_SurveyScanner, STATGROUP_Nightfall, SURVEYSCANNERRUNTIME_API);

class FSurveyScannerRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
