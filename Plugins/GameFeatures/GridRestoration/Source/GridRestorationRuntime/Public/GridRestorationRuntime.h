// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Nightfall.h"
#include "Stats/Stats.h"

GRIDRESTORATIONRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGridRestoration, Log, All);

/**
 * Declared against the core STATGROUP_Nightfall group, so this feature's cost appears in
 * the performance HUD's per-system list with no change to the HUD or the perf subsystem.
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Grid Restoration"), STAT_Nightfall_GridRestoration, STATGROUP_Nightfall, GRIDRESTORATIONRUNTIME_API);

class FGridRestorationRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
