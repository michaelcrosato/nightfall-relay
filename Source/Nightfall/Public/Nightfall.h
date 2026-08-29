// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Stats/Stats.h"

/** Primary log category for the Nightfall runtime module. */
NIGHTFALL_API DECLARE_LOG_CATEGORY_EXTERN(LogNightfall, Log, All);

/**
 * Stat group backing the per-system rows of the performance HUD.
 * Cycle counters declared against this group are read back on the game thread by
 * UNightfallPerfSubsystem and rendered without any additional plumbing.
 */
DECLARE_STATS_GROUP(TEXT("Nightfall"), STATGROUP_Nightfall, STATCAT_Advanced);

class FNightfallModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
