// Copyright Nightfall Relay. All Rights Reserved.

#include "Nightfall.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogNightfall);

void FNightfallModule::StartupModule()
{
	UE_LOG(LogNightfall, Log, TEXT("Nightfall runtime module online."));
}

void FNightfallModule::ShutdownModule()
{
	UE_LOG(LogNightfall, Log, TEXT("Nightfall runtime module offline."));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FNightfallModule, Nightfall, "Nightfall");
