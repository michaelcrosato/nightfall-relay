// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallRelayPylon.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NightfallGameplayTags.h"
#include "NightfallInteractableComponent.h"
#include "NightfallStats.h"

#define LOCTEXT_NAMESPACE "Nightfall"

ANightfallRelayPylon::ANightfallRelayPylon()
{
	// Parts are created in hierarchy order; a profile fills them in by these names.
	Base = CreatePart(TEXT("Base"), MachineRoot);
	Collar = CreatePart(TEXT("Collar"), Base);
	Mast = CreatePart(TEXT("Mast"), Base);
	RingLower = CreatePart(TEXT("RingLower"), Mast);
	RingUpper = CreatePart(TEXT("RingUpper"), Mast);
	Core = CreatePart(TEXT("Core"), Mast);

	// The core reads as the source of the light, so the point light lives inside it.
	CoreLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CoreLight"));
	CoreLight->SetupAttachment(Core);
	CoreLight->SetMobility(EComponentMobility::Movable);
	CoreLight->SetIntensityUnits(ELightUnits::Lumens);
	CoreLight->SetAttenuationRadius(InfluenceRadius);
	CoreLight->SetCastShadows(true);
	CoreLight->SetVolumetricScatteringIntensity(2.4f);
	CoreLight->SetIntensity(0.0f);

	// A second light pools on the ground. Together with the fog this is what makes an
	// energised pylon read from across the field.
	WashLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("WashLight"));
	WashLight->SetupAttachment(Mast);
	WashLight->SetMobility(EComponentMobility::Movable);
	WashLight->SetIntensityUnits(ELightUnits::Lumens);
	WashLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	WashLight->SetInnerConeAngle(22.0f);
	WashLight->SetOuterConeAngle(58.0f);
	WashLight->SetAttenuationRadius(InfluenceRadius);
	WashLight->SetCastShadows(true);
	WashLight->SetVolumetricScatteringIntensity(1.6f);
	WashLight->SetIntensity(0.0f);

	Interactable = CreateDefaultSubobject<UNightfallInteractableComponent>(TEXT("Interactable"));
	Interactable->SetupAttachment(Base);
	Interactable->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	Interactable->InteractionTags.AddTag(NightfallTags::Interactable_PowerNode);
	Interactable->DisplayName = LOCTEXT("PylonName", "Relay Pylon");
	Interactable->Verb = LOCTEXT("PylonVerb", "Energise");
	Interactable->HoldSeconds = 0.0f;
	Interactable->MaxInteractionDistanceOverride = 300.0f;
}

void ANightfallRelayPylon::BeginPlay()
{
	Super::BeginPlay();

	CoreLight->SetAttenuationRadius(InfluenceRadius);
	WashLight->SetAttenuationRadius(InfluenceRadius);

	bBroadcastOnline = IsOnline();
	SetStateTag(IsOnline()
		? NightfallTags::Power_Node_Online
		: (ChargeLevel > 0.0f ? NightfallTags::Power_Node_Charging : NightfallTags::Power_Node_Dormant));

	// Snap rather than interpolate, so a loaded save opens on the correct silhouette.
	MastHeight = FMath::SmoothStep(0.0f, 1.0f, ChargeLevel) * MastTravel;
	RefreshPresentation();
}

void ANightfallRelayPylon::OnNightfallPostLoad()
{
	// Charge came back as a raw number. Snap the mast and the lights to match it rather
	// than letting them interpolate up from wherever they happened to be.
	bBroadcastOnline = IsOnline();
	MastHeight = FMath::SmoothStep(0.0f, 1.0f, ChargeLevel) * MastTravel;
	SetStateTag(IsOnline()
		? NightfallTags::Power_Node_Online
		: (ChargeLevel > 0.0f ? NightfallTags::Power_Node_Charging : NightfallTags::Power_Node_Dormant));
	RefreshPresentation();
	OnChargeChanged.Broadcast(this, ChargeLevel);
}

float ANightfallRelayPylon::AddCharge(float Amount)
{
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float Before = ChargeLevel;
	SetChargeLevel(ChargeLevel + Amount);
	return ChargeLevel - Before;
}

void ANightfallRelayPylon::SetChargeLevel(float NewLevel)
{
	const float Clamped = FMath::Clamp(NewLevel, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Clamped, ChargeLevel))
	{
		return;
	}

	ChargeLevel = Clamped;
	OnChargeChanged.Broadcast(this, ChargeLevel);

	SetStateTag(IsOnline()
		? NightfallTags::Power_Node_Online
		: (ChargeLevel > 0.0f ? NightfallTags::Power_Node_Charging : NightfallTags::Power_Node_Dormant));

	if (IsOnline() && !bBroadcastOnline)
	{
		bBroadcastOnline = true;
		OnCameOnline.Broadcast(this);
	}
	else if (!IsOnline())
	{
		bBroadcastOnline = false;
	}
}

void ANightfallRelayPylon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Fixtures);

	// The mast chases the charge rather than snapping to it, so a delivery reads as the
	// structure hauling itself upright.
	const float TargetHeight = FMath::SmoothStep(0.0f, 1.0f, ChargeLevel) * MastTravel;
	MastHeight = FMath::FInterpTo(MastHeight, TargetHeight, DeltaSeconds, 1.9f);

	// Rings spin up with charge and keep turning once online.
	const float SpinRate = RingSpinSpeed * FMath::Max(ChargeLevel, IsOnline() ? 1.0f : 0.0f);
	RingAngle = FMath::Fmod(RingAngle + SpinRate * DeltaSeconds, 360.0f);

	if (IsOnline())
	{
		CollarAngle = FMath::Fmod(CollarAngle + CollarSpinSpeed * DeltaSeconds, 360.0f);
	}

	RefreshPresentation();
}

void ANightfallRelayPylon::RefreshPresentation()
{
	// Mast rides up out of the base; everything parented to it comes along.
	if (Mast)
	{
		FVector MastLocation = Mast->GetRelativeLocation();
		MastLocation.Z = MastHeight;
		Mast->SetRelativeLocation(MastLocation);
	}

	// Counter-rotation is what sells these as machinery rather than as a spinning prop.
	if (RingLower)
	{
		FRotator Rotation = RingLower->GetRelativeRotation();
		Rotation.Yaw = RingAngle;
		RingLower->SetRelativeRotation(Rotation);
	}
	if (RingUpper)
	{
		FRotator Rotation = RingUpper->GetRelativeRotation();
		Rotation.Yaw = -RingAngle * 0.62f;
		RingUpper->SetRelativeRotation(Rotation);
	}
	if (Collar)
	{
		FRotator Rotation = Collar->GetRelativeRotation();
		Rotation.Yaw = CollarAngle;
		Collar->SetRelativeRotation(Rotation);
	}

	// While charging the pylon flickers; online it holds steady. One scalar, whole machine.
	const float Flicker = IsOnline()
		? 1.0f
		: 0.72f + 0.28f * FMath::Sin(GetWorld()->GetTimeSeconds() * 11.0f);
	SetEmissiveLevel(ChargeLevel * Flicker);

	const FLinearColor Color = FMath::Lerp(ChargingColor, OnlineColor, ChargeLevel);
	SetEmissiveColor(Color);

	if (CoreLight)
	{
		CoreLight->SetIntensity(CoreLightIntensity * ChargeLevel * Flicker);
		CoreLight->SetLightColor(Color);
	}
	if (WashLight)
	{
		WashLight->SetIntensity(WashLightIntensity * ChargeLevel * Flicker);
		WashLight->SetLightColor(Color);
	}
}

#undef LOCTEXT_NAMESPACE
