// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "NightfallUpscaling.h"
#include "NightfallGameUserSettings.generated.h"

/**
 * Adds the upscaling and lighting-feature settings to Unreal's own graphics settings.
 *
 * Registered through GameUserSettingsClassName in DefaultEngine.ini, so
 * UGameUserSettings::GetGameUserSettings() returns this class everywhere and the standard
 * scalability API keeps working unchanged.
 *
 * The DLSS options here are always present in the menu. When the NVIDIA plugins are not
 * installed they report unavailable and the renderer runs TSR instead; see
 * NightfallUpscaling for why the integration is console-variable driven.
 */
UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class NIGHTFALL_API UNightfallGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UNightfallGameUserSettings();

	/** Typed accessor. Never null once the engine has initialised. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	static UNightfallGameUserSettings* GetNightfallSettings();

	virtual void SetToDefaults() override;
	virtual void ApplyNonResolutionSettings() override;

	// --- Upscaling -------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	ENightfallUpscaler GetUpscaler() const { return Upscaler; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetUpscaler(ENightfallUpscaler NewUpscaler) { Upscaler = NewUpscaler; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	ENightfallDlssQuality GetDlssQuality() const { return DlssQuality; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetDlssQuality(ENightfallDlssQuality NewQuality) { DlssQuality = NewQuality; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetRayReconstruction() const { return bRayReconstruction; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetRayReconstruction(bool bEnabled) { bRayReconstruction = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetFrameGeneration() const { return bFrameGeneration; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetFrameGeneration(bool bEnabled) { bFrameGeneration = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	ENightfallReflexMode GetReflexMode() const { return ReflexMode; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetReflexMode(ENightfallReflexMode NewMode) { ReflexMode = NewMode; }

	/** Render scale for the Native and TSR paths, as a percentage of output resolution. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	float GetNativeScreenPercentage() const { return NativeScreenPercentage; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetNativeScreenPercentage(float NewPercentage) { NativeScreenPercentage = FMath::Clamp(NewPercentage, 25.0f, 200.0f); }

	// --- Lighting features -----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetLumenHardwareRayTracing() const { return bLumenHardwareRayTracing; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetLumenHardwareRayTracing(bool bEnabled) { bLumenHardwareRayTracing = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetMegaLights() const { return bMegaLights; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetMegaLights(bool bEnabled) { bMegaLights = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetVirtualShadowMaps() const { return bVirtualShadowMaps; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetVirtualShadowMaps(bool bEnabled) { bVirtualShadowMaps = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetRayTracedTranslucency() const { return bRayTracedTranslucency; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetRayTracedTranslucency(bool bEnabled) { bRayTracedTranslucency = bEnabled; }

	/** Distance in cm volumetric fog is traced to. The single biggest fog cost. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	float GetVolumetricFogDistance() const { return VolumetricFogDistance; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetVolumetricFogDistance(float NewDistance) { VolumetricFogDistance = FMath::Clamp(NewDistance, 2000.0f, 60000.0f); }

	/** Invert vertical look, for players who fly rather than aim. Off is the standard. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Settings")
	bool GetInvertLookY() const { return bInvertLookY; }

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Settings")
	void SetInvertLookY(bool bInvert) { bInvertLookY = bInvert; }

private:
	UPROPERTY(Config)
	ENightfallUpscaler Upscaler;

	UPROPERTY(Config)
	ENightfallDlssQuality DlssQuality;

	UPROPERTY(Config)
	bool bRayReconstruction;

	UPROPERTY(Config)
	bool bFrameGeneration;

	UPROPERTY(Config)
	ENightfallReflexMode ReflexMode;

	UPROPERTY(Config)
	float NativeScreenPercentage;

	UPROPERTY(Config)
	bool bLumenHardwareRayTracing;

	UPROPERTY(Config)
	bool bMegaLights;

	UPROPERTY(Config)
	bool bVirtualShadowMaps;

	UPROPERTY(Config)
	bool bRayTracedTranslucency;

	UPROPERTY(Config)
	float VolumetricFogDistance;

	UPROPERTY(Config)
	bool bInvertLookY;
};
