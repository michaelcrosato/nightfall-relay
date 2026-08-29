// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Nightfall.h"
#include "Stats/Stats.h"

/**
 * Per-system cycle counters surfaced by the performance HUD.
 *
 * Everything declared against STATGROUP_Nightfall is discovered by
 * UNightfallPerfSubsystem automatically, so a Game Feature Plugin that declares its
 * own counter against this group appears in the HUD with no core edit. See
 * ADDING_A_FEATURE.md for the worked example.
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("World Clock"), STAT_Nightfall_WorldClock, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Sky Director"), STAT_Nightfall_SkyDirector, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Machines"), STAT_Nightfall_Machines, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Fixtures"), STAT_Nightfall_Fixtures, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("WPO Fields"), STAT_Nightfall_WpoFields, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Interaction"), STAT_Nightfall_Interaction, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Player"), STAT_Nightfall_Player, STATGROUP_Nightfall, NIGHTFALL_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Telemetry"), STAT_Nightfall_Telemetry, STATGROUP_Nightfall, NIGHTFALL_API);
