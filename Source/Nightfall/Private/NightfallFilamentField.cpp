// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallFilamentField.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NightfallDustSubsystem.h"
#include "NightfallStats.h"
#include "NightfallWorldClockSubsystem.h"

ANightfallFilamentField::ANightfallFilamentField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Filaments = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Filaments"));
	SetRootComponent(Filaments);

	// Filaments are visual only. They must never cost a query, and they must not cast
	// shadows - a few thousand shadow-casting blades would eat the virtual shadow map
	// budget for no visible gain at this scale.
	Filaments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Filaments->SetCastShadow(false);
	Filaments->SetMobility(EComponentMobility::Static);

	// World position offset is the whole point, but evaluating it forever is wasteful.
	// Past this distance the blades hold still, which nobody can see.
	Filaments->SetWorldPositionOffsetDisableDistance(9000);
	Filaments->InstanceStartCullDistance = 9000;
	Filaments->InstanceEndCullDistance = 13000;
}

void ANightfallFilamentField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildField();
}

void ANightfallFilamentField::BeginPlay()
{
	Super::BeginPlay();

	if (Filaments->GetInstanceCount() == 0)
	{
		RebuildField();
	}

	WindMaterial = Filaments->CreateAndSetMaterialInstanceDynamic(0);

	// Ground under a filament bed is held together and raises no dust. The blades have no
	// collision by design, so a downward trace passes straight through them and reports the
	// terrain underneath - the footprint has to be declared rather than discovered.
	if (UNightfallDustSubsystem* Dust = UNightfallDustSubsystem::Get(this))
	{
		Dust->RegisterCleanZone(this, FVector2D(GetActorLocation()), Extent);
	}
}

void ANightfallFilamentField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNightfallDustSubsystem* Dust = UNightfallDustSubsystem::Get(this))
	{
		Dust->UnregisterCleanZone(this);
	}

	Super::EndPlay(EndPlayReason);
}

int32 ANightfallFilamentField::GetPlacedCount() const
{
	return Filaments ? Filaments->GetInstanceCount() : 0;
}

void ANightfallFilamentField::RebuildField()
{
	if (!Filaments)
	{
		return;
	}

	Filaments->ClearInstances();

	if (!FilamentMesh || InstanceCount <= 0)
	{
		return;
	}

	Filaments->SetStaticMesh(FilamentMesh);

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// One stream, seeded once: the field is reproducible on every load and on every
	// machine, which is what lets it be rebuilt rather than serialised.
	FRandomStream Stream(Seed);
	const FTransform ActorTransform = GetActorTransform();
	const float CosMaxSlope = FMath::Cos(FMath::DegreesToRadians(MaxGroundSlope));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NightfallFilamentScatter), /*bTraceComplex=*/true, this);

	TArray<FTransform> Placements;
	Placements.Reserve(InstanceCount);

	for (int32 Index = 0; Index < InstanceCount; ++Index)
	{
		const FVector LocalPoint(
			Stream.FRandRange(-Extent.X, Extent.X),
			Stream.FRandRange(-Extent.Y, Extent.Y),
			0.0f);

		const FVector WorldPoint = ActorTransform.TransformPosition(LocalPoint);
		const FVector TraceStart = WorldPoint + FVector(0.0f, 0.0f, TraceHeight);
		const FVector TraceEnd = WorldPoint - FVector(0.0f, 0.0f, TraceHeight);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			continue;
		}

		// Refuse steep ground; filaments growing sideways out of a cliff read as an error.
		if (Hit.ImpactNormal.Z < CosMaxSlope)
		{
			continue;
		}

		// Stand upright regardless of slope, with a random yaw and a slight lean.
		const FRotator Rotation(
			Stream.FRandRange(-6.0f, 6.0f),
			Stream.FRandRange(0.0f, 360.0f),
			Stream.FRandRange(-6.0f, 6.0f));

		const float Scale = Stream.FRandRange(MinScale, MaxScale);

		Placements.Emplace(
			Rotation,
			ActorTransform.InverseTransformPosition(Hit.ImpactPoint),
			FVector(Scale));
	}

	if (Placements.Num() > 0)
	{
		Filaments->AddInstances(Placements, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);
	}
}

void ANightfallFilamentField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_WpoFields);

	if (!WindMaterial)
	{
		return;
	}

	GustTime += DeltaSeconds;

	// Three incommensurate oscillators. Their sum never repeats over any period a player
	// will sit still for, which is enough to read as weather rather than as a loop.
	const float Slow = FMath::Sin(GustTime * GustBaseFrequency * UE_TWO_PI);
	const float Mid = FMath::Sin(GustTime * GustBaseFrequency * 2.7f * UE_TWO_PI + 1.7f);
	const float Fast = FMath::Sin(GustTime * GustBaseFrequency * 6.3f * UE_TWO_PI + 4.1f);

	const float GustNormalised = (Slow * 0.55f + Mid * 0.30f + Fast * 0.15f) * 0.5f + 0.5f;
	const float Gust = WindStrength + GustAmplitude * GustNormalised;

	// The bearing wanders with the slow oscillator only, so gusts arrive from a direction
	// that drifts rather than jitters.
	const float Bearing = WindBearingDegrees + Slow * BearingWanderDegrees;
	const float BearingRadians = FMath::DegreesToRadians(Bearing);
	const FVector WindVector(FMath::Cos(BearingRadians), FMath::Sin(BearingRadians), 0.0f);

	WindMaterial->SetVectorParameterValue(WindVectorParameter, FLinearColor(WindVector));
	WindMaterial->SetScalarParameterValue(GustStrengthParameter, Gust);

	// Filaments carry a faint glow that only shows once the sun is down, which gives the
	// ground something to read against at night without adding a single light.
	//
	// It runs on the sky profile's own night-ward window and easing, so the blades arrive at
	// full glow on the frame the Night key does. The previous linear ramp over +6..-8 had
	// them at 30% on the first frame of the game under twenty-five thousand lux of sun, and
	// still climbing a quarter of a minute after the sky had stopped.
	float NightGlow = 0.0f;
	if (const UNightfallWorldClockSubsystem* Clock = GetWorld()->GetSubsystem<UNightfallWorldClockSubsystem>())
	{
		const float Altitude = Clock->GetSunAltitudeDegrees();
		NightGlow = 1.0f - FMath::SmoothStep(GlowFullAltitudeDegrees, GlowStartAltitudeDegrees, Altitude);
	}
	WindMaterial->SetScalarParameterValue(NightGlowParameter, NightGlow * NightGlowStrength);
}
