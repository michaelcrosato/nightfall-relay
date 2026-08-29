// Copyright Nightfall Relay. All Rights Reserved.

#include "GridRestorationRuntime.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGridRestoration);
DEFINE_STAT(STAT_Nightfall_GridRestoration);

void FGridRestorationRuntimeModule::StartupModule()
{
	UE_LOG(LogGridRestoration, Log, TEXT("Grid Restoration feature module loaded."));
}

void FGridRestorationRuntimeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FGridRestorationRuntimeModule, GridRestorationRuntime);
