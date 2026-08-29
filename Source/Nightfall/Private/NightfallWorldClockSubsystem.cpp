// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallWorldClockSubsystem.h"

#include "HAL/IConsoleManager.h"
#include "Nightfall.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallStats.h"

namespace
{
	/** Altitude below which the sky is treated as fully night, in degrees. */
	constexpr float NightAltitudeDegrees = -6.0f;

	/** Altitude above which the sky is treated as full day, in degrees. */
	constexpr float DayAltitudeDegrees = 10.0f;

	/** Altitude at and above which the ground is taking all the sun it is going to. */
	constexpr float FullSolarLoadAltitudeDegrees = 20.0f;

	/** How far back the thermal model looks. Four lag constants; beyond that nothing is left. */
	constexpr float TemperatureLookbackHours = 12.0f;

	/** Samples across that window. Sixteen is smooth to well under a tenth of a degree. */
	constexpr int32 TemperatureSampleCount = 16;

	/** Floor on the lag constant, so a misconfigured zero cannot divide by nothing. */
	constexpr float MinThermalLagHours = 0.25f;

	/** Spelled out rather than pasted, so the encoding of this file cannot change it. */
	const TCHAR* const DegreeSign = TEXT("\u00B0");

	/**
	 * Read a YYYY-MM-DD date. Deliberately not FDateTime::Parse, which refuses anything
	 * without a time on it, and the config line is a date.
	 */
	bool ParseStartDate(const FString& Text, FDateTime& OutDate)
	{
		TArray<FString> Tokens;
		Text.ParseIntoArray(Tokens, TEXT("-"), /*InCullEmpty=*/true);
		if (Tokens.Num() != 3)
		{
			return false;
		}

		const int32 Year = FCString::Atoi(*Tokens[0]);
		const int32 Month = FCString::Atoi(*Tokens[1]);
		const int32 Day = FCString::Atoi(*Tokens[2]);
		if (!FDateTime::Validate(Year, Month, Day, 0, 0, 0, 0))
		{
			return false;
		}

		OutDate = FDateTime(Year, Month, Day);
		return true;
	}

	const TCHAR* const MonthNames[] = {
		TEXT("JAN"), TEXT("FEB"), TEXT("MAR"), TEXT("APR"), TEXT("MAY"), TEXT("JUN"),
		TEXT("JUL"), TEXT("AUG"), TEXT("SEP"), TEXT("OCT"), TEXT("NOV"), TEXT("DEC")
	};
}

void UNightfallWorldClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNightfallRuntimeSettings& Settings = UNightfallRuntimeSettings::Get();
	DayLengthMinutes = Settings.DayLengthMinutes;
	TimeOfDayHours = FMath::Fmod(FMath::Max(Settings.StartTimeOfDayHours, 0.0f), 24.0f);
	SunriseAzimuthDegrees = Settings.SunriseAzimuthDegrees;
	MaxSunAltitudeDegrees = Settings.MaxSunAltitudeDegrees;
	NightTemperatureCelsius = Settings.NightTemperatureCelsius;
	DayTemperatureCelsius = Settings.DayTemperatureCelsius;
	ThermalLagHours = FMath::Max(Settings.ThermalLagHours, MinThermalLagHours);

	if (!ParseStartDate(Settings.StartDate, StartDate))
	{
		UE_LOG(LogNightfall, Warning,
			TEXT("StartDate '%s' is not a YYYY-MM-DD date; opening on %s instead."),
			*Settings.StartDate, *StartDate.ToString(TEXT("%Y-%m-%d")));
	}

	RecomputeSolarState();
	RecomputeTemperature();
	CurrentPhase = ClassifyPhase(SunAltitudeDegrees, IsMorning());

	// Scrubbing time is the single most useful thing to be able to do while working on
	// the lighting, so it gets a console command rather than living behind a Blueprint.
	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.SetTime"),
		TEXT("Nightfall.SetTime <hours> - jump the clock, 0 to 24."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				SetTimeOfDayHours(FCString::Atof(*Args[0]));
				UE_LOG(LogNightfall, Log,
					TEXT("Clock set to %s %s (sun altitude %.1f degrees, %.1f C)."),
					*GetDateString(), *GetClockString(), SunAltitudeDegrees, TemperatureCelsius);
			}
		}),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.SetDay"),
		TEXT("Nightfall.SetDay <index> - jump to a day, counting from 0. The hour is left alone."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				SetDayIndex(FCString::Atoi(*Args[0]));
				UE_LOG(LogNightfall, Log, TEXT("Clock set to %s %s."), *GetDateString(), *GetClockString());
			}
		}),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.PauseTime"),
		TEXT("Nightfall.PauseTime <0|1> - stop or resume the day cycle."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			SetPaused(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			UE_LOG(LogNightfall, Log, TEXT("Clock %s."), bPaused ? TEXT("paused") : TEXT("running"));
		}),
		ECVF_Default));
}

void UNightfallWorldClockSubsystem::Deinitialize()
{
	for (IConsoleObject* Object : ConsoleObjects)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleObjects.Empty();

	Super::Deinitialize();
}

bool UNightfallWorldClockSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UNightfallWorldClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNightfallWorldClockSubsystem, STATGROUP_Tickables);
}

void UNightfallWorldClockSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_WorldClock);

	if (!bPaused && DayLengthMinutes > KINDA_SMALL_NUMBER)
	{
		const float HoursPerSecond = 24.0f / (DayLengthMinutes * 60.0f);
		const float Advanced = TimeOfDayHours + DeltaTime * HoursPerSecond;

		// Midnight is caught rather than discarded, which is the whole of the calendar:
		// the date is the start date plus however many times this has fired.
		DayIndex += FMath::FloorToInt(Advanced / 24.0f);
		TimeOfDayHours = FMath::Fmod(Advanced, 24.0f);
	}

	RecomputeSolarState();
	RecomputeTemperature();
	UpdatePhaseAndBroadcast();
}

void UNightfallWorldClockSubsystem::SetTimeOfDayHours(float NewHours)
{
	TimeOfDayHours = FMath::Fmod(FMath::Fmod(NewHours, 24.0f) + 24.0f, 24.0f);
	RecomputeSolarState();
	RecomputeTemperature();
	UpdatePhaseAndBroadcast();
}

void UNightfallWorldClockSubsystem::SetDayIndex(int32 NewDayIndex)
{
	DayIndex = FMath::Max(NewDayIndex, 0);
}

void UNightfallWorldClockSubsystem::AdvanceHours(float DeltaHours)
{
	// Advancing past either end of the day moves the date with it, so a feature that runs
	// the clock forward by hand gets the same calendar the tick would have produced.
	const float Total = TimeOfDayHours + DeltaHours;
	SetDayIndex(DayIndex + FMath::FloorToInt(Total / 24.0f));
	SetTimeOfDayHours(Total);
}

void UNightfallWorldClockSubsystem::SetDayLengthMinutes(float NewLength)
{
	DayLengthMinutes = FMath::Max(NewLength, 0.5f);
}

void UNightfallWorldClockSubsystem::RecomputeSolarState()
{
	// The sun rides a circle in the XZ plane: minus a quarter turn at midnight, plus a
	// quarter turn at noon.
	const float PhaseRadians = (TimeOfDayHours / 24.0f) * UE_TWO_PI - UE_HALF_PI;
	FVector Direction(FMath::Cos(PhaseRadians), 0.0f, FMath::Sin(PhaseRadians));

	// Roll that circle away from vertical so noon peaks at MaxSunAltitudeDegrees rather
	// than straight overhead, then swing the whole arc round to the authored bearing.
	const float TiltDegrees = 90.0f - MaxSunAltitudeDegrees;
	Direction = FRotator(0.0f, 0.0f, TiltDegrees).RotateVector(Direction);
	Direction = FRotator(0.0f, SunriseAzimuthDegrees, 0.0f).RotateVector(Direction);

	SunDirection = Direction.GetSafeNormal();
	SunAltitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(SunDirection.Z), -1.0f, 1.0f)));
}

void UNightfallWorldClockSubsystem::UpdatePhaseAndBroadcast()
{
	const ENightfallTimePhase NewPhase = ClassifyPhase(SunAltitudeDegrees, IsMorning());
	if (NewPhase != CurrentPhase)
	{
		CurrentPhase = NewPhase;
		OnTimePhaseChanged.Broadcast(CurrentPhase);
	}
}

ENightfallTimePhase UNightfallWorldClockSubsystem::ClassifyPhase(float AltitudeDegrees, bool bMorning)
{
	if (AltitudeDegrees >= DayAltitudeDegrees)
	{
		return ENightfallTimePhase::Day;
	}
	if (AltitudeDegrees <= NightAltitudeDegrees)
	{
		return ENightfallTimePhase::Night;
	}
	return bMorning ? ENightfallTimePhase::Dawn : ENightfallTimePhase::Dusk;
}

float UNightfallWorldClockSubsystem::SunAltitudeDegreesAtHour(float Hours) const
{
	// The arc RecomputeSolarState builds, solved for its height alone. The azimuth swing is
	// a yaw and leaves height untouched, and rolling the circle away from vertical scales
	// it by exactly the sine of the peak altitude - so no vector work is needed to ask what
	// the sun was doing at some other hour.
	const float PhaseRadians = (Hours / 24.0f) * UE_TWO_PI - UE_HALF_PI;
	const float SinAltitude = FMath::Sin(PhaseRadians) * FMath::Sin(FMath::DegreesToRadians(MaxSunAltitudeDegrees));
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SinAltitude, -1.0f, 1.0f)));
}

float UNightfallWorldClockSubsystem::SolarLoadAtHour(float Hours) const
{
	const float Altitude = SunAltitudeDegreesAtHour(Hours);
	return FMath::Clamp(
		(Altitude - NightAltitudeDegrees) / (FullSolarLoadAltitudeDegrees - NightAltitudeDegrees),
		0.0f, 1.0f);
}

void UNightfallWorldClockSubsystem::RecomputeTemperature()
{
	// Weight the hours behind this one by how much of their heat the ground has kept, and
	// average the sun that fell in them. Looking backwards rather than filtering forwards
	// keeps the answer a pure function of the hour: scrubbing the clock lands on the
	// temperature the day would have reached had it run there on its own, and pausing does
	// not freeze the model out of step with the sky.
	float WeightedLoad = 0.0f;
	float TotalWeight = 0.0f;

	for (int32 Index = 0; Index < TemperatureSampleCount; ++Index)
	{
		const float HoursBack = TemperatureLookbackHours * Index / (TemperatureSampleCount - 1);
		const float Weight = FMath::Exp(-HoursBack / ThermalLagHours);

		WeightedLoad += Weight * SolarLoadAtHour(TimeOfDayHours - HoursBack);
		TotalWeight += Weight;
	}

	const float Load = (TotalWeight > KINDA_SMALL_NUMBER) ? (WeightedLoad / TotalWeight) : 0.0f;
	TemperatureCelsius = FMath::Lerp(NightTemperatureCelsius, DayTemperatureCelsius, Load);
}

FDateTime UNightfallWorldClockSubsystem::GetDateTime() const
{
	return StartDate + FTimespan::FromDays(DayIndex) + FTimespan::FromHours(TimeOfDayHours);
}

FString UNightfallWorldClockSubsystem::GetClockString() const
{
	const int32 Hours = FMath::Clamp(FMath::FloorToInt(TimeOfDayHours), 0, 23);
	const int32 Minutes = FMath::Clamp(FMath::FloorToInt((TimeOfDayHours - Hours) * 60.0f), 0, 59);
	return FString::Printf(TEXT("%02d:%02d"), Hours, Minutes);
}

FString UNightfallWorldClockSubsystem::GetDateString() const
{
	const FDateTime Date = GetDateTime();
	const int32 MonthIndex = FMath::Clamp(Date.GetMonth() - 1, 0, UE_ARRAY_COUNT(MonthNames) - 1);
	return FString::Printf(TEXT("%02d %s %04d"), Date.GetDay(), MonthNames[MonthIndex], Date.GetYear());
}

float UNightfallWorldClockSubsystem::GetTemperatureInDisplayUnit() const
{
	return (UNightfallRuntimeSettings::Get().TemperatureUnit == ENightfallTemperatureUnit::Fahrenheit)
		? TemperatureCelsius * 1.8f + 32.0f
		: TemperatureCelsius;
}

FString UNightfallWorldClockSubsystem::GetTemperatureString() const
{
	const bool bFahrenheit =
		UNightfallRuntimeSettings::Get().TemperatureUnit == ENightfallTemperatureUnit::Fahrenheit;

	// Rounded to an integer before printing rather than with %.0f, which would render the
	// last fraction of a degree below zero as "-0".
	return FString::Printf(TEXT("%d%s%s"),
		FMath::RoundToInt(GetTemperatureInDisplayUnit()),
		DegreeSign,
		bFahrenheit ? TEXT("F") : TEXT("C"));
}
