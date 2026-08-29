// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallSkyProfile.h"

FNightfallSkyKey FNightfallSkyKey::Blend(const FNightfallSkyKey& A, const FNightfallSkyKey& B, float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);

	FNightfallSkyKey Result;
	Result.SunIntensityLux = FMath::Lerp(A.SunIntensityLux, B.SunIntensityLux, T);
	Result.SunColor = FMath::Lerp(A.SunColor, B.SunColor, T);
	Result.SunSourceAngleDegrees = FMath::Lerp(A.SunSourceAngleDegrees, B.SunSourceAngleDegrees, T);
	Result.SunVolumetricScattering = FMath::Lerp(A.SunVolumetricScattering, B.SunVolumetricScattering, T);
	// A bool cannot be interpolated, and switching it at the midpoint popped the light
	// shafts on at half strength. Enable them whenever either key wants them and let the
	// scale carry the strength, so the flag only ever flips where the scale is already at
	// zero and nothing changes on screen when it does.
	Result.bSunLightShaftBloom = A.bSunLightShaftBloom || B.bSunLightShaftBloom;
	Result.SunLightShaftBloomScale = FMath::Lerp(
		A.bSunLightShaftBloom ? A.SunLightShaftBloomScale : 0.0f,
		B.bSunLightShaftBloom ? B.SunLightShaftBloomScale : 0.0f, T);
	Result.MoonIntensityLux = FMath::Lerp(A.MoonIntensityLux, B.MoonIntensityLux, T);
	Result.MoonColor = FMath::Lerp(A.MoonColor, B.MoonColor, T);
	Result.SkyLightIntensity = FMath::Lerp(A.SkyLightIntensity, B.SkyLightIntensity, T);
	Result.SkyLightColor = FMath::Lerp(A.SkyLightColor, B.SkyLightColor, T);
	Result.SkyLightVolumetricScattering = FMath::Lerp(A.SkyLightVolumetricScattering, B.SkyLightVolumetricScattering, T);
	Result.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, T);
	Result.FogHeightFalloff = FMath::Lerp(A.FogHeightFalloff, B.FogHeightFalloff, T);
	Result.FogInscatteringColor = FMath::Lerp(A.FogInscatteringColor, B.FogInscatteringColor, T);
	Result.VolumetricFogExtinctionScale = FMath::Lerp(A.VolumetricFogExtinctionScale, B.VolumetricFogExtinctionScale, T);
	Result.VolumetricFogAlbedo = FMath::Lerp(A.VolumetricFogAlbedo, B.VolumetricFogAlbedo, T);
	Result.VolumetricFogEmissive = FMath::Lerp(A.VolumetricFogEmissive, B.VolumetricFogEmissive, T);
	Result.VolumetricFogScatteringDistribution = FMath::Lerp(A.VolumetricFogScatteringDistribution, B.VolumetricFogScatteringDistribution, T);
	Result.ExposureBias = FMath::Lerp(A.ExposureBias, B.ExposureBias, T);
	return Result;
}

UNightfallSkyProfile::UNightfallSkyProfile()
{
	// A note on the moon colours, which look far more saturated than the sun's beside them.
	// They are not a style inconsistency: they are the values the night was actually
	// calibrated against. The sky director used to hand light colours to the engine
	// unencoded while every reader decoded them as sRGB, so a pale authored blue arrived a
	// whole gamma deeper. When that was corrected the sun improved - dusk went from near
	// pure red to the sodium amber it was always meant to be - but the night lost its cold
	// cast and washed out to violet. So the moon carries what the correct path now needs to
	// reproduce the calibrated look, rather than the pale value that only ever worked by
	// accident. The sun keys are untouched and mean exactly what they say.

	// --- Full day. Bright, near neutral, thin fog so distance still reads. ------------
	Day.SunIntensityLux = 75000.0f;
	Day.SunColor = FLinearColor(1.0f, 0.972f, 0.925f);
	Day.SunSourceAngleDegrees = 0.62f;
	Day.SunVolumetricScattering = 1.0f;
	Day.bSunLightShaftBloom = false;
	Day.SunLightShaftBloomScale = 0.12f;
	Day.MoonIntensityLux = 0.0f;
	Day.MoonColor = FLinearColor(0.216f, 0.342f, 1.0f);
	Day.SkyLightIntensity = 1.20f;
	Day.SkyLightColor = FLinearColor(0.88f, 0.93f, 1.0f);
	Day.SkyLightVolumetricScattering = 1.0f;
	Day.FogDensity = 0.012f;
	Day.FogHeightFalloff = 0.25f;
	Day.FogInscatteringColor = FLinearColor(0.090f, 0.130f, 0.190f);
	Day.VolumetricFogExtinctionScale = 0.60f;
	Day.VolumetricFogAlbedo = FLinearColor(1.0f, 1.0f, 1.0f);
	Day.VolumetricFogEmissive = FLinearColor::Black;
	Day.VolumetricFogScatteringDistribution = 0.30f;
	Day.ExposureBias = 0.0f;

	// --- Dusk. The signature state: sodium sun raking across cold concrete. -----------
	Dusk.SunIntensityLux = 21000.0f;
	Dusk.SunColor = FLinearColor(1.0f, 0.520f, 0.205f);
	Dusk.SunSourceAngleDegrees = 2.30f;
	Dusk.SunVolumetricScattering = 2.60f;
	Dusk.bSunLightShaftBloom = true;
	Dusk.SunLightShaftBloomScale = 0.36f;
	Dusk.MoonIntensityLux = 0.55f;
	Dusk.MoonColor = FLinearColor(0.216f, 0.342f, 1.0f);
	Dusk.SkyLightIntensity = 0.62f;
	Dusk.SkyLightColor = FLinearColor(0.35f, 0.45f, 0.72f);
	Dusk.SkyLightVolumetricScattering = 1.30f;
	Dusk.FogDensity = 0.028f;
	Dusk.FogHeightFalloff = 0.18f;
	Dusk.FogInscatteringColor = FLinearColor(0.220f, 0.120f, 0.080f);
	Dusk.VolumetricFogExtinctionScale = 1.45f;
	Dusk.VolumetricFogAlbedo = FLinearColor(1.0f, 0.86f, 0.72f);
	Dusk.VolumetricFogEmissive = FLinearColor::Black;
	Dusk.VolumetricFogScatteringDistribution = 0.62f;
	Dusk.ExposureBias = 0.15f;

	// --- Dawn. Same geometry as dusk but the colour swings cold rather than hot. ------
	Dawn.SunIntensityLux = 17500.0f;
	Dawn.SunColor = FLinearColor(0.950f, 0.620f, 0.430f);
	Dawn.SunSourceAngleDegrees = 2.05f;
	Dawn.SunVolumetricScattering = 2.10f;
	Dawn.bSunLightShaftBloom = true;
	Dawn.SunLightShaftBloomScale = 0.28f;
	Dawn.MoonIntensityLux = 0.55f;
	Dawn.MoonColor = FLinearColor(0.216f, 0.342f, 1.0f);
	Dawn.SkyLightIntensity = 0.66f;
	Dawn.SkyLightColor = FLinearColor(0.40f, 0.50f, 0.76f);
	Dawn.SkyLightVolumetricScattering = 1.25f;
	Dawn.FogDensity = 0.026f;
	Dawn.FogHeightFalloff = 0.18f;
	Dawn.FogInscatteringColor = FLinearColor(0.140f, 0.145f, 0.185f);
	Dawn.VolumetricFogExtinctionScale = 1.35f;
	Dawn.VolumetricFogAlbedo = FLinearColor(0.92f, 0.95f, 1.0f);
	Dawn.VolumetricFogEmissive = FLinearColor::Black;
	Dawn.VolumetricFogScatteringDistribution = 0.55f;
	Dawn.ExposureBias = 0.10f;

	// --- Night. Sun off entirely; emissives and MegaLights carry the frame. -----------
	Night.SunIntensityLux = 0.0f;
	Night.SunColor = FLinearColor(0.30f, 0.36f, 0.55f);
	Night.SunSourceAngleDegrees = 0.62f;
	Night.SunVolumetricScattering = 0.0f;
	Night.bSunLightShaftBloom = false;
	Night.SunLightShaftBloomScale = 0.0f;
	Night.MoonIntensityLux = 0.85f;
	Night.MoonColor = FLinearColor(0.195f, 0.319f, 1.0f);
	Night.SkyLightIntensity = 0.115f;
	Night.SkyLightColor = FLinearColor(0.25f, 0.35f, 0.62f);
	Night.SkyLightVolumetricScattering = 0.60f;
	Night.FogDensity = 0.035f;
	Night.FogHeightFalloff = 0.15f;
	Night.FogInscatteringColor = FLinearColor(0.012f, 0.017f, 0.030f);
	Night.VolumetricFogExtinctionScale = 1.85f;
	Night.VolumetricFogAlbedo = FLinearColor(0.70f, 0.80f, 1.0f);
	Night.VolumetricFogEmissive = FLinearColor(0.00050f, 0.00080f, 0.00150f);
	Night.VolumetricFogScatteringDistribution = 0.10f;
	Night.ExposureBias = 0.25f;
}

FNightfallSkyKey UNightfallSkyProfile::Evaluate(float SunAltitudeDegrees, bool bMorning) const
{
	if (SunAltitudeDegrees >= DayAltitudeDegrees)
	{
		return Day;
	}
	if (SunAltitudeDegrees <= NightAltitudeDegrees)
	{
		return Night;
	}

	const FNightfallSkyKey& Twilight = bMorning ? Dawn : Dusk;

	// SmoothStep rather than a straight ratio: a linear blend arrives at each key with the
	// sky still moving at full rate and then stops dead, which reads as the light snapping
	// to a halt at the horizon and again at the night and day ends. Easing both ends puts a
	// zero derivative at every key, so the whole cycle changes without a seam.
	if (SunAltitudeDegrees < 0.0f)
	{
		// Below the horizon but inside civil twilight: night bleeding into the warm key.
		const float Alpha = FMath::SmoothStep(NightAltitudeDegrees, 0.0f, SunAltitudeDegrees);
		return FNightfallSkyKey::Blend(Night, Twilight, Alpha);
	}

	const float Alpha = FMath::SmoothStep(0.0f, DayAltitudeDegrees, SunAltitudeDegrees);
	return FNightfallSkyKey::Blend(Twilight, Day, Alpha);
}
