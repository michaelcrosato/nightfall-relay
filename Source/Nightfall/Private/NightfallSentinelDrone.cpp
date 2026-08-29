// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallSentinelDrone.h"

#include "Components/LocalFogVolumeComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NightfallConspicuous.h"
#include "NightfallDustSubsystem.h"
#include "NightfallGameplayTags.h"
#include "NightfallStats.h"

namespace
{
	/** Rotor spin at rest, in degrees per second. */
	constexpr float RotorIdleSpeed = 900.0f;

	/** Extra rotor spin at full cruise, in degrees per second. */
	constexpr float RotorLoadSpeed = 1500.0f;
}

ANightfallSentinelDrone::ANightfallSentinelDrone()
{
	Hull = CreatePart(TEXT("Hull"), MachineRoot);

	// Sensor rig: yaw ring carries the pitch arm, which carries the pod. Exactly the
	// turret hierarchy, hanging under the hull.
	YawRing = CreatePart(TEXT("YawRing"), Hull);
	PitchArm = CreatePart(TEXT("PitchArm"), YawRing);
	SensorPod = CreatePart(TEXT("SensorPod"), PitchArm);

	static const TCHAR* RotorNames[] = { TEXT("RotorA"), TEXT("RotorB"), TEXT("RotorC"), TEXT("RotorD") };
	for (const TCHAR* Name : RotorNames)
	{
		Rotors.Add(CreatePart(Name, Hull));
	}

	SensorBeam = CreateDefaultSubobject<USpotLightComponent>(TEXT("SensorBeam"));
	SensorBeam->SetupAttachment(SensorPod);
	SensorBeam->SetMobility(EComponentMobility::Movable);
	SensorBeam->SetIntensityUnits(ELightUnits::Lumens);
	SensorBeam->SetInnerConeAngle(8.0f);
	SensorBeam->SetOuterConeAngle(19.0f);
	SensorBeam->SetAttenuationRadius(3200.0f);
	SensorBeam->SetCastShadows(true);
	SensorBeam->SetVolumetricScatteringIntensity(3.2f);
	SensorBeam->SetIntensity(BeamIntensity);

	// Rotor wash. Attached to the machine root rather than the hull, and held in world
	// space, so the plume stays flat on the ground while the hull banks into its turns.
	// Both extinctions start at zero, which the renderer skips outright.
	Downwash = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("Downwash"));
	Downwash->SetupAttachment(MachineRoot);
	Downwash->SetUsingAbsoluteLocation(true);
	Downwash->SetUsingAbsoluteRotation(true);
	Downwash->RadialFogExtinction = 0.0f;
	Downwash->HeightFogExtinction = 0.0f;
	Downwash->HeightFogFalloff = 3000.0f;
	Downwash->HeightFogOffset = 0.0f;
	Downwash->FogPhaseG = 0.62f;
	Downwash->FogAlbedo = FLinearColor(0.66f, 0.58f, 0.48f);
	// Ahead of the ambient haze, so a plume is never the volume dropped by the per-view cap.
	Downwash->FogSortPriority = 40;

	// The hull blocks the world but is never pushed by it.
	Hull->SetCollisionObjectType(ECC_WorldDynamic);
	Hull->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void ANightfallSentinelDrone::BeginPlay()
{
	Super::BeginPlay();

	ApplyTuningRow();

	BaseLocation = GetActorLocation();

	// Offsets are authored in the drone's own space so a route can be copied between
	// placements and still make sense.
	WorldRoute.Reset();
	const FTransform SpawnTransform = GetActorTransform();
	for (const FVector& Offset : PatrolOffsets)
	{
		WorldRoute.Add(SpawnTransform.TransformPosition(Offset));
	}

	// Stagger the hover so a group of drones does not pulse in unison.
	HoverPhase = FMath::Fmod(GetActorLocation().X * 0.011f + GetActorLocation().Y * 0.017f, UE_TWO_PI);

	SetStateTag(NightfallTags::Machine_State_Patrol);
	SetEmissiveLevel(1.0f);
	SensorBeam->SetLightColor(PatrolBeamColor);
}

void ANightfallSentinelDrone::ApplyTuningRow()
{
	if (!TuningTable || TuningRowName.IsNone())
	{
		return;
	}

	const FNightfallSentinelTuningRow* Row = TuningTable->FindRow<FNightfallSentinelTuningRow>(
		TuningRowName, TEXT("ANightfallSentinelDrone"), /*bWarnIfRowMissing=*/true);
	if (!Row)
	{
		return;
	}

	PatrolSpeed = Row->PatrolSpeed;
	AlertSpeed = Row->AlertSpeed;
	DetectionRange = Row->DetectionRange;
	DetectionHalfAngle = Row->DetectionHalfAngle;
	TimeToAcquire = Row->TimeToAcquire;
	TimeToLose = Row->TimeToLose;
	InvestigateDuration = Row->InvestigateDuration;
	BeamIntensity = Row->BeamIntensity;
}

void ANightfallSentinelDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Machines);

	UpdateSensing(DeltaSeconds);
	UpdateMovement(DeltaSeconds);
	UpdateSensorRig(DeltaSeconds);
	UpdateRotors(DeltaSeconds);
	UpdateDownwash(DeltaSeconds);
}

void ANightfallSentinelDrone::UpdateDownwash(float DeltaSeconds)
{
	if (!Downwash)
	{
		return;
	}

	// The ground is sampled on a timer rather than every frame. A drone crosses 340 cm/s on
	// patrol, so at six a second the sample is never more than half a metre stale.
	GroundTraceTimer -= DeltaSeconds;
	if (GroundTraceTimer <= 0.0f)
	{
		GroundTraceTimer = DownwashTraceInterval;

		const FVector Start = BaseLocation;
		const FVector End = Start - FVector(0.0f, 0.0f, 4000.0f);

		FCollisionQueryParams Params(SCENE_QUERY_STAT(NightfallDroneDownwash), /*bTraceComplex=*/false, this);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			GroundZ = Hit.ImpactPoint.Z;

			// Terrain carries the dusty tag; the compound platform and the structures do not.
			const AActor* HitActor = Hit.GetActor();
			float Dustiness = (HitActor && HitActor->ActorHasTag(DustySurfaceTag)) ? 1.0f : 0.0f;

			// Grass has no collision, so the beds have to be asked rather than hit.
			if (const UNightfallDustSubsystem* Dust = UNightfallDustSubsystem::Get(this))
			{
				Dustiness = FMath::Min(Dustiness, Dust->GetGroundDustiness(Hit.ImpactPoint));
			}
			GroundDustiness = Dustiness;
		}
		else
		{
			GroundDustiness = 0.0f;
		}
	}

	// Height above the ground, from the un-bobbed body position: using the actor location
	// would pulse the plume with the idle hover.
	const float Altitude = BaseLocation.Z - GroundZ;
	const float AltitudeAlpha = FMath::GetMappedRangeValueClamped(
		FVector2f(DownwashMaxAltitude, DownwashMinAltitude), FVector2f(0.0f, 1.0f), Altitude);

	// Eased rather than snapped, so crossing onto pavement or climbing away thins the plume
	// out instead of switching it off.
	DustAlpha = FMath::FInterpTo(DustAlpha, AltitudeAlpha * GroundDustiness, DeltaSeconds, 2.5f);

	// A transform change is one render command. Setting a density marks the render state
	// dirty and rebuilds the volume's scene proxy, so that one is quantised below.
	Downwash->SetWorldLocation(FVector(BaseLocation.X, BaseLocation.Y, GroundZ + 40.0f));

	// Low and hard when it is close to the ground, broad and thin when it is high - the
	// same reason a real rotor wash tightens as it descends.
	const float Radius = FMath::Lerp(DownwashHighRadius, DownwashLowRadius, AltitudeAlpha);
	Downwash->SetWorldScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));

	const float Extinction = DustAlpha * DownwashMaxExtinction;
	if (!FMath::IsNearlyEqual(Extinction, LastAppliedExtinction, 0.02f))
	{
		LastAppliedExtinction = Extinction;
		Downwash->SetHeightFogExtinction(Extinction);
		Downwash->SetRadialFogExtinction(Extinction * 0.30f);
	}
}

// --- Sensing --------------------------------------------------------------------------

bool ANightfallSentinelDrone::HasLineOfSightToTarget(FVector& OutTargetLocation, float& OutConspicuity) const
{
	OutConspicuity = 0.0f;

	const APawn* Target = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Target)
	{
		return false;
	}

	// Everything the drone knows about the target beyond its position: how much light it is
	// carrying. The interface keeps this a question about a conspicuous thing rather than
	// about a player, so the drone still has no idea what a phone or a power cell is.
	if (const INightfallConspicuous* Conspicuous = Cast<INightfallConspicuous>(Target))
	{
		OutConspicuity = FMath::Clamp(Conspicuous->GetConspicuity(), 0.0f, 1.0f);
	}

	const FVector EyeLocation = SensorPod ? SensorPod->GetComponentLocation() : GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - EyeLocation;

	// A lit figure is picked out further away than a dark one. Geometry alone would make
	// crossing bare ground with the light on no worse than crossing it dark.
	const float EffectiveRange = DetectionRange * (1.0f + ConspicuousRangeBonus * OutConspicuity);

	const float Distance = ToTarget.Size();
	if (Distance > EffectiveRange || Distance < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Cone test against the hull's facing rather than the pod's, so a drone cannot see
	// behind itself simply because the sensor happened to be swept round.
	const FVector Forward = Hull ? Hull->GetForwardVector() : GetActorForwardVector();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(DetectionHalfAngle));
	if (FVector::DotProduct(Forward, ToTarget / Distance) < CosHalfAngle)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NightfallDroneSight), /*bTraceComplex=*/false, this);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TargetLocation, ECC_Visibility, Params))
	{
		return false;
	}

	OutTargetLocation = TargetLocation;
	return true;
}

void ANightfallSentinelDrone::UpdateSensing(float DeltaSeconds)
{
	FVector SeenLocation;
	float Conspicuity = 0.0f;
	const bool bSees = HasLineOfSightToTarget(SeenLocation, Conspicuity);

	if (bSees)
	{
		LastKnownTargetLocation = SeenLocation;
		UnsightSeconds = 0.0f;

		// Something lit resolves faster, so the bar fills quicker rather than the threshold
		// moving. Losing sight still resets it to zero, so the light costs you the window
		// you had, not the whole encounter.
		SightSeconds += DeltaSeconds * (1.0f + ConspicuousAcquireBonus * Conspicuity);

		if (!bAlerted && SightSeconds >= TimeToAcquire)
		{
			SetAlerted(true);
		}
		return;
	}

	SightSeconds = 0.0f;

	if (!bAlerted)
	{
		// Run down the investigation timer; when it expires, back to the route.
		if (InvestigateRemaining > 0.0f)
		{
			InvestigateRemaining -= DeltaSeconds;
			if (InvestigateRemaining <= 0.0f)
			{
				SetStateTag(NightfallTags::Machine_State_Patrol);
			}
		}
		return;
	}

	UnsightSeconds += DeltaSeconds;
	if (UnsightSeconds >= TimeToLose)
	{
		SetAlerted(false);
		InvestigateRemaining = InvestigateDuration;
		SetStateTag(NightfallTags::Machine_State_Investigate);
	}
}

void ANightfallSentinelDrone::SetAlerted(bool bNewAlerted)
{
	if (bAlerted == bNewAlerted)
	{
		return;
	}

	bAlerted = bNewAlerted;
	SightSeconds = 0.0f;
	UnsightSeconds = 0.0f;

	if (bAlerted)
	{
		SetStateTag(NightfallTags::Machine_State_Alert);
	}

	const FLinearColor BeamColor = bAlerted ? AlertBeamColor : PatrolBeamColor;
	SensorBeam->SetLightColor(BeamColor);
	SetEmissiveColor(BeamColor);

	OnAlertChanged.Broadcast(this, bAlerted);
}

void ANightfallSentinelDrone::ResetToPatrol()
{
	SetAlerted(false);
	InvestigateRemaining = 0.0f;
	SetStateTag(NightfallTags::Machine_State_Patrol);
}

// --- Movement -------------------------------------------------------------------------

FVector ANightfallSentinelDrone::ResolveDesiredLocation() const
{
	if (bAlerted)
	{
		// Hold a standoff distance rather than flying into the player's face.
		const FVector ToTarget = LastKnownTargetLocation - BaseLocation;
		const float Distance = ToTarget.Size();
		if (Distance > 900.0f)
		{
			return LastKnownTargetLocation - ToTarget.GetSafeNormal() * 800.0f;
		}
		return BaseLocation;
	}

	if (InvestigateRemaining > 0.0f)
	{
		return LastKnownTargetLocation + FVector(0.0f, 0.0f, 350.0f);
	}

	if (WorldRoute.IsValidIndex(RouteIndex))
	{
		return WorldRoute[RouteIndex];
	}

	return BaseLocation;
}

void ANightfallSentinelDrone::UpdateMovement(float DeltaSeconds)
{
	const FVector Desired = ResolveDesiredLocation();
	const FVector ToDesired = Desired - BaseLocation;
	const float Distance = ToDesired.Size();

	// Advance the route once the current leg is reached.
	if (!bAlerted && InvestigateRemaining <= 0.0f && WorldRoute.Num() > 1 && Distance <= WaypointTolerance)
	{
		RouteIndex = (RouteIndex + 1) % WorldRoute.Num();
	}

	const float Speed = bAlerted ? AlertSpeed : PatrolSpeed;
	FVector TargetVelocity = FVector::ZeroVector;
	if (Distance > KINDA_SMALL_NUMBER)
	{
		// Ease into the last metre so arrivals settle instead of snapping.
		const float Throttle = FMath::Clamp(Distance / 260.0f, 0.0f, 1.0f);
		TargetVelocity = (ToDesired / Distance) * Speed * Throttle;
	}

	// Smoothing the velocity is what gives the drone mass; it also feeds the bank angle.
	SmoothedVelocity = FMath::VInterpTo(SmoothedVelocity, TargetVelocity, DeltaSeconds, 2.4f);
	BaseLocation += SmoothedVelocity * DeltaSeconds;

	// Hover bob rides on top of the travel, never fighting it.
	HoverPhase = FMath::Fmod(HoverPhase + DeltaSeconds * HoverFrequency * UE_TWO_PI, UE_TWO_PI);
	const FVector Bob(0.0f, 0.0f, FMath::Sin(HoverPhase) * HoverAmplitude);
	SetActorLocation(BaseLocation + Bob, /*bSweep=*/false);

	// Face travel, and bank into it. Both are rigid rotations on the actor and hull.
	const FVector Heading = SmoothedVelocity.GetSafeNormal2D();
	if (!Heading.IsNearlyZero())
	{
		const FRotator TargetRotation(0.0f, Heading.Rotation().Yaw, 0.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, 3.2f));
	}

	if (Hull)
	{
		const float SpeedRatio = FMath::Clamp(SmoothedVelocity.Size() / FMath::Max(PatrolSpeed, 1.0f), 0.0f, 1.5f);
		const FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(SmoothedVelocity);
		const float BankRoll = FMath::Clamp(LocalVelocity.Y / FMath::Max(PatrolSpeed, 1.0f), -1.0f, 1.0f) * MaxBankDegrees;
		const float NosePitch = -SpeedRatio * MaxBankDegrees * 0.45f;

		const FRotator TargetHull(NosePitch, 0.0f, BankRoll);
		Hull->SetRelativeRotation(FMath::RInterpTo(Hull->GetRelativeRotation(), TargetHull, DeltaSeconds, 3.6f));
	}
}

// --- Sensor rig and rotors --------------------------------------------------------------

void ANightfallSentinelDrone::UpdateSensorRig(float DeltaSeconds)
{
	float DesiredYaw = 0.0f;
	float DesiredPitch = 0.0f;

	if (bAlerted || InvestigateRemaining > 0.0f)
	{
		// Track: convert the target into hull space and read the angles straight off.
		const FVector Local = Hull
			? Hull->GetComponentTransform().InverseTransformPosition(LastKnownTargetLocation)
			: GetActorTransform().InverseTransformPosition(LastKnownTargetLocation);

		const FRotator Look = Local.Rotation();
		DesiredYaw = Look.Yaw;
		DesiredPitch = FMath::Clamp(Look.Pitch, -80.0f, 25.0f);
	}
	else
	{
		// Search: a slow sweep, with the pod angled down at the ground it is covering.
		SweepPhase = FMath::Fmod(SweepPhase + DeltaSeconds * 0.42f, UE_TWO_PI);
		DesiredYaw = FMath::Sin(SweepPhase) * SearchSweepDegrees;
		DesiredPitch = -24.0f;
	}

	const float TrackRate = bAlerted ? 7.5f : 2.2f;
	SensorYaw = FMath::FInterpTo(SensorYaw, DesiredYaw, DeltaSeconds, TrackRate);
	SensorPitch = FMath::FInterpTo(SensorPitch, DesiredPitch, DeltaSeconds, TrackRate);

	if (YawRing)
	{
		YawRing->SetRelativeRotation(FRotator(0.0f, SensorYaw, 0.0f));
	}
	if (PitchArm)
	{
		PitchArm->SetRelativeRotation(FRotator(SensorPitch, 0.0f, 0.0f));
	}

	if (SensorBeam)
	{
		// Pulse the beam while alerted so it reads from a distance.
		const float Pulse = bAlerted
			? 0.70f + 0.30f * FMath::Sin(GetWorld()->GetTimeSeconds() * 9.0f)
			: 1.0f;
		SensorBeam->SetIntensity(BeamIntensity * Pulse);
	}
}

void ANightfallSentinelDrone::UpdateRotors(float DeltaSeconds)
{
	const float SpeedRatio = FMath::Clamp(SmoothedVelocity.Size() / FMath::Max(PatrolSpeed, 1.0f), 0.0f, 1.5f);
	RotorAngle = FMath::Fmod(RotorAngle + (RotorIdleSpeed + RotorLoadSpeed * SpeedRatio) * DeltaSeconds, 360.0f);

	// Alternate direction so opposing pairs counter-torque, which is both correct and
	// easier to read than four discs spinning the same way.
	for (int32 Index = 0; Index < Rotors.Num(); ++Index)
	{
		if (UStaticMeshComponent* Rotor = Rotors[Index].Get())
		{
			const float Direction = (Index % 2 == 0) ? 1.0f : -1.0f;
			Rotor->SetRelativeRotation(FRotator(0.0f, RotorAngle * Direction, 0.0f));
		}
	}
}
