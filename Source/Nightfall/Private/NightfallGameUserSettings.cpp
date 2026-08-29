// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallGameUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Nightfall.h"

namespace
{
	void SetConsoleVariable(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls=*/false))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
		else
		{
			UE_LOG(LogNightfall, Warning, TEXT("Console variable '%s' not found; setting had no effect."), Name);
		}
	}

	void SetConsoleVariable(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls=*/false))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
		else
		{
			UE_LOG(LogNightfall, Warning, TEXT("Console variable '%s' not found; setting had no effect."), Name);
		}
	}
}

UNightfallGameUserSettings::UNightfallGameUserSettings()
{
	SetToDefaults();
}

UNightfallGameUserSettings* UNightfallGameUserSettings::GetNightfallSettings()
{
	return Cast<UNightfallGameUserSettings>(GEngine ? GEngine->GetGameUserSettings() : nullptr);
}

void UNightfallGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	// Defaults describe the target configuration from the brief: a 4070 Super at 1440p
	// holding 60 fps before frame generation, at DLSS Quality.
	Upscaler = ENightfallUpscaler::DLSS;
	DlssQuality = ENightfallDlssQuality::Quality;
	bRayReconstruction = true;
	bFrameGeneration = false;
	ReflexMode = ENightfallReflexMode::Enabled;
	NativeScreenPercentage = 66.7f;

	bLumenHardwareRayTracing = true;
	bMegaLights = true;
	bVirtualShadowMaps = true;
	bRayTracedTranslucency = true;
	VolumetricFogDistance = 22000.0f;
	bInvertLookY = false;
}

void UNightfallGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	// Lighting features first: the upscaler's cost depends on what they leave behind.
	SetConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"), bLumenHardwareRayTracing ? 1 : 0);
	SetConsoleVariable(TEXT("r.MegaLights.Allowed"), bMegaLights ? 1 : 0);
	SetConsoleVariable(TEXT("r.Shadow.Virtual.Enable"), bVirtualShadowMaps ? 1 : 0);
	SetConsoleVariable(TEXT("r.Lumen.TranslucencyReflections.FrontLayer.Allow"), bRayTracedTranslucency ? 1 : 0);

	// Volumetric fog distance is a component property rather than a console variable, so
	// ANightfallSkyDirector reads it back from here every frame.

	NightfallUpscaling::ApplyUpscaler(Upscaler, DlssQuality, NativeScreenPercentage);
	NightfallUpscaling::ApplyRayReconstruction(bRayReconstruction);
	NightfallUpscaling::ApplyFrameGeneration(bFrameGeneration);
	NightfallUpscaling::ApplyReflex(ReflexMode);
}
