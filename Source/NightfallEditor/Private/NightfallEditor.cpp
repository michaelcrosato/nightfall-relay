// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallEditor.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogNightfallEditor);

void FNightfallEditorModule::StartupModule()
{
	UE_LOG(LogNightfallEditor, Log, TEXT("Nightfall editor module online."));
}

void FNightfallEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FNightfallEditorModule, NightfallEditor);
