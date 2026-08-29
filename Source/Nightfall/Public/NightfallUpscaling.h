// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NightfallUpscaling.generated.h"

/** Which upscaler produces the final image. */
UENUM(BlueprintType)
enum class ENightfallUpscaler : uint8
{
	/** Native resolution, engine anti-aliasing only. */
	Native				UMETA(DisplayName = "Native (no upscaling)"),

	/** Temporal Super Resolution. Always available; the fallback when DLSS is not installed. */
	TSR					UMETA(DisplayName = "TSR"),

	/** DLSS Super Resolution, rendering below output resolution. */
	DLSS				UMETA(DisplayName = "DLSS Super Resolution"),

	/** DLAA: the DLSS network at full resolution, as anti-aliasing rather than upscaling. */
	DLAA				UMETA(DisplayName = "DLAA")
};

/** DLSS Super Resolution quality preset. Ignored unless the upscaler is DLSS. */
UENUM(BlueprintType)
enum class ENightfallDlssQuality : uint8
{
	UltraPerformance	UMETA(DisplayName = "Ultra Performance"),
	Performance			UMETA(DisplayName = "Performance"),
	Balanced			UMETA(DisplayName = "Balanced"),
	Quality				UMETA(DisplayName = "Quality"),
	UltraQuality		UMETA(DisplayName = "Ultra Quality")
};

/** NVIDIA Reflex low latency mode. */
UENUM(BlueprintType)
enum class ENightfallReflexMode : uint8
{
	Off					UMETA(DisplayName = "Off"),
	Enabled				UMETA(DisplayName = "On"),
	EnabledPlusBoost	UMETA(DisplayName = "On + Boost")
};

/**
 * Thin control layer over the NVIDIA DLSS and Streamline Unreal plugins.
 *
 * Everything here goes through console variables rather than through the plugins' own
 * headers, and nothing in this project links against them. That is deliberate for two
 * reasons: the game should not carry a hard dependency on a vendor plugin, and the plugin
 * binaries are distributed from NVIDIA behind an account login, so a fresh clone must
 * build and run without them.
 *
 * Every function below detects whether the relevant console variable exists. If the
 * plugins are absent the DLSS paths report unavailable, the settings menu greys them out
 * with a reason, and the renderer falls back to TSR. Drop the official DLSS and
 * Streamline plugins into Plugins/ and the same code drives them with no changes.
 *
 * Console variable names follow NVIDIA's published Unreal plugin documentation:
 *   r.NGX.Enable, r.NGX.DLSS.Enable, r.NGX.DLSS.Quality, r.NGX.DLSS.DenoiserMode,
 *   r.Streamline.DLSSG.Enable, r.Streamline.Reflex.Mode
 */
namespace NightfallUpscaling
{
	/** True when the DLSS plugin is present and its console variables are registered. */
	NIGHTFALL_API bool IsDlssAvailable();

	/** True when DLSS Ray Reconstruction can be driven. */
	NIGHTFALL_API bool IsRayReconstructionAvailable();

	/** True when Streamline frame generation can be driven. */
	NIGHTFALL_API bool IsFrameGenerationAvailable();

	/** True when Streamline Reflex can be driven. */
	NIGHTFALL_API bool IsReflexAvailable();

	/** One line explaining what is and is not installed, for the settings menu. */
	NIGHTFALL_API FText GetAvailabilitySummary();

	/**
	 * Select the upscaler.
	 *
	 * @param Upscaler              Which path to use.
	 * @param Quality               DLSS preset; used only when Upscaler is DLSS.
	 * @param NativeScreenPercentage Render scale for the Native and TSR paths, as a percentage.
	 */
	NIGHTFALL_API void ApplyUpscaler(ENightfallUpscaler Upscaler, ENightfallDlssQuality Quality, float NativeScreenPercentage);

	/** Enable or disable DLSS Ray Reconstruction. */
	NIGHTFALL_API void ApplyRayReconstruction(bool bEnabled);

	/** Enable or disable 2x frame generation. */
	NIGHTFALL_API void ApplyFrameGeneration(bool bEnabled);

	/** Set the Reflex low latency mode. */
	NIGHTFALL_API void ApplyReflex(ENightfallReflexMode Mode);

	/** Screen percentage a DLSS quality preset corresponds to, for display in the menu. */
	NIGHTFALL_API float GetDlssScreenPercentage(ENightfallDlssQuality Quality);
}
