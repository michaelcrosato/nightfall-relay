// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallDustSubsystem.h"

#include "Engine/World.h"
#include "NightfallRuntimeSettings.h"

UNightfallDustSubsystem* UNightfallDustSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNightfallDustSubsystem>() : nullptr;
}

bool UNightfallDustSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNightfallDustSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The compound is levelled and surfaced, and it is the one clean area that owns no actor
	// able to register itself. Its footprint is a project setting so it can be retuned from
	// the ini without a rebuild of any kind.
	const UNightfallRuntimeSettings& Settings = UNightfallRuntimeSettings::Get();
	if (!Settings.CompoundCleanHalfExtent.IsNearlyZero())
	{
		RegisterCleanZone(nullptr, FVector2D::ZeroVector, Settings.CompoundCleanHalfExtent);
	}
}

void UNightfallDustSubsystem::RegisterCleanZone(const AActor* Owner, const FVector2D& Center, const FVector2D& HalfExtent)
{
	FCleanZone& Zone = CleanZones.AddDefaulted_GetRef();
	Zone.Owner = Owner;
	Zone.Center = Center;
	Zone.HalfExtent = HalfExtent;
}

void UNightfallDustSubsystem::UnregisterCleanZone(const AActor* Owner)
{
	CleanZones.RemoveAll([Owner](const FCleanZone& Zone)
	{
		return Zone.Owner.Get() == Owner;
	});
}

float UNightfallDustSubsystem::GetGroundDustiness(const FVector& WorldLocation) const
{
	const FVector2D Location(WorldLocation);
	const float Feather = FMath::Max(FeatherDistance, KINDA_SMALL_NUMBER);

	float Dustiness = 1.0f;
	for (const FCleanZone& Zone : CleanZones)
	{
		// Distance from the rectangle, measured per axis and combined: zero inside, growing
		// outward. Cheaper than a real distance field and exact enough for a fog boundary.
		const FVector2D Delta = (Location - Zone.Center).GetAbs() - Zone.HalfExtent;
		const float Outside = FVector2D(FMath::Max(Delta.X, 0.0f), FMath::Max(Delta.Y, 0.0f)).Size();

		Dustiness = FMath::Min(Dustiness, FMath::SmoothStep(0.0f, Feather, Outside));
		if (Dustiness <= 0.0f)
		{
			break;
		}
	}

	return Dustiness;
}
