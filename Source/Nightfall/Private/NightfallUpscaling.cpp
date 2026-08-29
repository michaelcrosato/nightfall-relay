// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallUpscaling.h"

#include "HAL/IConsoleManager.h"
#include "Nightfall.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace NightfallUpscaling
{
namespace
{
	// --- NVIDIA plugin console variables -----------------------------------------------
	const TCHAR* CVarNgxEnable = TEXT("r.NGX.Enable");
	const TCHAR* CVarDlssEnable = TEXT("r.NGX.DLSS.Enable");
	const TCHAR* CVarDlssQuality = TEXT("r.NGX.DLSS.Quality");
	const TCHAR* CVarDlssDenoiserMode = TEXT("r.NGX.DLSS.DenoiserMode");
	const TCHAR* CVarFrameGeneration = TEXT("r.Streamline.DLSSG.Enable");
	const TCHAR* CVarReflexMode = TEXT("r.Streamline.Reflex.Mode");

	// --- Engine console variables ------------------------------------------------------
	const TCHAR* CVarAntiAliasingMethod = TEXT("r.AntiAliasingMethod");
	const TCHAR* CVarScreenPercentage = TEXT("r.ScreenPercentage");

	/** r.AntiAliasingMethod: 0 none, 1 FXAA, 2 TAA, 3 MSAA, 4 TSR. */
	constexpr int32 AntiAliasingMethodTSR = 4;

	IConsoleVariable* Find(const TCHAR* Name)
	{
		return IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls=*/false);
	}

	void SetIfPresent(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Variable = Find(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}

	void SetIfPresent(const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* Variable = Find(Name))
		{
			Variable->Set(Value, ECVF_SetByGameSetting);
		}
	}

	/**
	 * r.NGX.DLSS.Quality follows NVIDIA's ordering, where negative values are the more
	 * aggressive presets.
	 */
	int32 ToDlssQualityValue(ENightfallDlssQuality Quality)
	{
		switch (Quality)
		{
		case ENightfallDlssQuality::UltraPerformance:	return -2;
		case ENightfallDlssQuality::Performance:		return -1;
		case ENightfallDlssQuality::Balanced:			return 0;
		case ENightfallDlssQuality::Quality:			return 1;
		case ENightfallDlssQuality::UltraQuality:		return 2;
		default:										return 1;
		}
	}
}

bool IsDlssAvailable()
{
	return Find(CVarDlssEnable) != nullptr && Find(CVarDlssQuality) != nullptr;
}

bool IsRayReconstructionAvailable()
{
	return Find(CVarDlssDenoiserMode) != nullptr;
}

bool IsFrameGenerationAvailable()
{
	return Find(CVarFrameGeneration) != nullptr;
}

bool IsReflexAvailable()
{
	return Find(CVarReflexMode) != nullptr;
}

FText GetAvailabilitySummary()
{
	if (!IsDlssAvailable())
	{
		return LOCTEXT("DlssMissing",
			"NVIDIA DLSS plugin not installed - using TSR. Add the DLSS and Streamline plugins to Plugins/ to enable these options.");
	}

	if (!IsFrameGenerationAvailable())
	{
		return LOCTEXT("StreamlineMissing",
			"DLSS available. Streamline plugin not installed, so frame generation and Reflex are unavailable.");
	}

	return LOCTEXT("DlssPresent",
		"DLSS and Streamline available. Frame generation on this GPU is 2x; multi frame generation requires RTX 50 series.");
}

float GetDlssScreenPercentage(ENightfallDlssQuality Quality)
{
	// NVIDIA's published render scales for each preset.
	switch (Quality)
	{
	case ENightfallDlssQuality::UltraPerformance:	return 33.0f;
	case ENightfallDlssQuality::Performance:		return 50.0f;
	case ENightfallDlssQuality::Balanced:			return 58.0f;
	case ENightfallDlssQuality::Quality:			return 66.7f;
	case ENightfallDlssQuality::UltraQuality:		return 77.0f;
	default:										return 66.7f;
	}
}

void ApplyUpscaler(ENightfallUpscaler Upscaler, ENightfallDlssQuality Quality, float NativeScreenPercentage)
{
	const bool bDlssAvailable = IsDlssAvailable();
	const bool bWantsDlss = (Upscaler == ENightfallUpscaler::DLSS || Upscaler == ENightfallUpscaler::DLAA);

	if (bWantsDlss && !bDlssAvailable)
	{
		// Asking for DLSS without the plugin is not an error the player should have to
		// resolve. Fall back, say so once, and carry on at the same output resolution.
		UE_LOG(LogNightfall, Warning,
			TEXT("DLSS requested but the NVIDIA plugin is not installed. Falling back to TSR."));
		Upscaler = ENightfallUpscaler::TSR;
	}

	switch (Upscaler)
	{
	case ENightfallUpscaler::DLSS:
		SetIfPresent(CVarNgxEnable, 1);
		SetIfPresent(CVarDlssEnable, 1);
		SetIfPresent(CVarDlssQuality, ToDlssQualityValue(Quality));
		// The plugin owns screen percentage in this mode; leaving it at 100 lets the
		// quality preset decide the render scale.
		SetIfPresent(CVarScreenPercentage, 100.0f);
		break;

	case ENightfallUpscaler::DLAA:
		// DLAA is the DLSS network with no downscale: plugin on, render scale at native.
		SetIfPresent(CVarNgxEnable, 1);
		SetIfPresent(CVarDlssEnable, 1);
		SetIfPresent(CVarDlssQuality, ToDlssQualityValue(ENightfallDlssQuality::UltraQuality));
		SetIfPresent(CVarScreenPercentage, 100.0f);
		break;

	case ENightfallUpscaler::TSR:
		SetIfPresent(CVarDlssEnable, 0);
		SetIfPresent(CVarAntiAliasingMethod, AntiAliasingMethodTSR);
		SetIfPresent(CVarScreenPercentage, FMath::Clamp(NativeScreenPercentage, 25.0f, 200.0f));
		break;

	case ENightfallUpscaler::Native:
	default:
		SetIfPresent(CVarDlssEnable, 0);
		SetIfPresent(CVarAntiAliasingMethod, AntiAliasingMethodTSR);
		SetIfPresent(CVarScreenPercentage, 100.0f);
		break;
	}
}

void ApplyRayReconstruction(bool bEnabled)
{
	// Ray Reconstruction replaces the engine denoisers with the DLSS one. It only means
	// anything while DLSS itself is running.
	SetIfPresent(CVarDlssDenoiserMode, bEnabled ? 1 : 0);
}

void ApplyFrameGeneration(bool bEnabled)
{
	// RTX 40 series supports 2x generation only; multi frame generation is 50 series and
	// is not exposed here.
	SetIfPresent(CVarFrameGeneration, bEnabled ? 1 : 0);
}

void ApplyReflex(ENightfallReflexMode Mode)
{
	int32 Value = 0;
	switch (Mode)
	{
	case ENightfallReflexMode::Enabled:				Value = 1; break;
	case ENightfallReflexMode::EnabledPlusBoost:	Value = 2; break;
	case ENightfallReflexMode::Off:
	default:										Value = 0; break;
	}
	SetIfPresent(CVarReflexMode, Value);
}

} // namespace NightfallUpscaling

#undef LOCTEXT_NAMESPACE
