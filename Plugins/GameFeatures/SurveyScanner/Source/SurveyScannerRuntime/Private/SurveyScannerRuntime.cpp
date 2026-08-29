// Copyright Nightfall Relay. All Rights Reserved.

#include "SurveyScannerRuntime.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogSurveyScanner);
DEFINE_STAT(STAT_Nightfall_SurveyScanner);

void FSurveyScannerRuntimeModule::StartupModule()
{
	UE_LOG(LogSurveyScanner, Log, TEXT("Survey Scanner feature module loaded."));
}

void FSurveyScannerRuntimeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FSurveyScannerRuntimeModule, SurveyScannerRuntime);
