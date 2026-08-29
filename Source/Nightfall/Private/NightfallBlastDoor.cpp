// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallBlastDoor.h"

#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NightfallGameplayTags.h"
#include "NightfallInteractableComponent.h"
#include "NightfallStats.h"

#define LOCTEXT_NAMESPACE "Nightfall"

ANightfallBlastDoor::ANightfallBlastDoor()
{
	Frame = CreatePart(TEXT("Frame"), MachineRoot);
	LeafLeft = CreatePart(TEXT("LeafLeft"), Frame);
	LeafRight = CreatePart(TEXT("LeafRight"), Frame);
	LockWheel = CreatePart(TEXT("LockWheel"), Frame);
	StatusPanel = CreatePart(TEXT("StatusPanel"), Frame);

	// A rect light in the threshold: cheap, and it makes an open door legible from range.
	ThresholdLight = CreateDefaultSubobject<URectLightComponent>(TEXT("ThresholdLight"));
	ThresholdLight->SetupAttachment(Frame);
	ThresholdLight->SetMobility(EComponentMobility::Movable);
	ThresholdLight->SetIntensityUnits(ELightUnits::Lumens);
	ThresholdLight->SetSourceWidth(240.0f);
	ThresholdLight->SetSourceHeight(30.0f);
	ThresholdLight->SetAttenuationRadius(1400.0f);
	ThresholdLight->SetCastShadows(true);
	ThresholdLight->SetVolumetricScatteringIntensity(1.8f);
	ThresholdLight->SetIntensity(0.0f);
	ThresholdLight->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
	ThresholdLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));

	Interactable = CreateDefaultSubobject<UNightfallInteractableComponent>(TEXT("Interactable"));
	Interactable->SetupAttachment(Frame);
	Interactable->SetRelativeLocation(FVector(60.0f, 0.0f, 130.0f));
	Interactable->InteractionTags.AddTag(NightfallTags::Interactable_Fixture);
	Interactable->DisplayName = LOCTEXT("DoorName", "Blast Door");
	Interactable->Verb = LOCTEXT("DoorVerbOpen", "Open");
	Interactable->MaxInteractionDistanceOverride = 260.0f;
}

void ANightfallBlastDoor::BeginPlay()
{
	Super::BeginPlay();

	Interactable->OnInteracted.AddDynamic(this, &ANightfallBlastDoor::HandleInteracted);

	SetStateTag(bLocked
		? NightfallTags::Fixture_State_Locked
		: (OpenAmount >= 1.0f ? NightfallTags::Fixture_State_Open : NightfallTags::Fixture_State_Closed));

	RefreshPrompt();
	RefreshPresentation();
}

void ANightfallBlastDoor::HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor)
{
	Toggle();
}

void ANightfallBlastDoor::OnNightfallPostLoad()
{
	// A door restored mid-travel settles to the nearer end rather than resuming a motion
	// nobody asked for.
	TravelDirection = 0.0f;
	AutoCloseRemaining = 0.0f;
	OpenAmount = (OpenAmount >= 0.5f) ? 1.0f : 0.0f;

	Interactable->bInteractionEnabled = !bLocked;
	SetStateTag(bLocked
		? NightfallTags::Fixture_State_Locked
		: (OpenAmount >= 1.0f ? NightfallTags::Fixture_State_Open : NightfallTags::Fixture_State_Closed));
	RefreshPrompt();
	RefreshPresentation();
}

void ANightfallBlastDoor::Open()
{
	if (bLocked || OpenAmount >= 1.0f)
	{
		return;
	}

	TravelDirection = 1.0f;
	SetStateTag(NightfallTags::Fixture_State_Opening);
	RefreshPrompt();
}

void ANightfallBlastDoor::Close()
{
	if (OpenAmount <= 0.0f)
	{
		return;
	}

	TravelDirection = -1.0f;
	AutoCloseRemaining = 0.0f;
	SetStateTag(NightfallTags::Fixture_State_Closing);
	RefreshPrompt();
}

void ANightfallBlastDoor::Toggle()
{
	// Mid-travel, reverse. Otherwise go to the state we are not already in.
	if (TravelDirection > 0.0f || OpenAmount >= 1.0f)
	{
		Close();
	}
	else
	{
		Open();
	}
}

void ANightfallBlastDoor::SetLocked(bool bNewLocked)
{
	if (bLocked == bNewLocked)
	{
		return;
	}

	bLocked = bNewLocked;

	if (bLocked)
	{
		Close();
		SetStateTag(NightfallTags::Fixture_State_Locked);
	}

	Interactable->bInteractionEnabled = !bLocked;
	RefreshPrompt();
	RefreshPresentation();
}

void ANightfallBlastDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Fixtures);

	if (!FMath::IsNearlyZero(TravelDirection))
	{
		const float Step = DeltaSeconds / FMath::Max(TravelSeconds, 0.1f);
		OpenAmount = FMath::Clamp(OpenAmount + TravelDirection * Step, 0.0f, 1.0f);

		if (OpenAmount >= 1.0f)
		{
			TravelDirection = 0.0f;
			AutoCloseRemaining = AutoCloseDelay;
			SetStateTag(NightfallTags::Fixture_State_Open);
			RefreshPrompt();
		}
		else if (OpenAmount <= 0.0f)
		{
			TravelDirection = 0.0f;
			SetStateTag(bLocked ? NightfallTags::Fixture_State_Locked : NightfallTags::Fixture_State_Closed);
			RefreshPrompt();
		}
	}
	else if (AutoCloseRemaining > 0.0f)
	{
		AutoCloseRemaining -= DeltaSeconds;
		if (AutoCloseRemaining <= 0.0f)
		{
			Close();
		}
	}

	RefreshPresentation();
}

void ANightfallBlastDoor::RefreshPresentation()
{
	// Ease the travel so the leaves start and stop under load rather than linearly.
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, OpenAmount);
	const float Offset = Eased * LeafTravel;

	if (LeafLeft)
	{
		FVector Location = LeafLeft->GetRelativeLocation();
		Location.Y = -Offset;
		LeafLeft->SetRelativeLocation(Location);
	}
	if (LeafRight)
	{
		FVector Location = LeafRight->GetRelativeLocation();
		Location.Y = Offset;
		LeafRight->SetRelativeLocation(Location);
	}
	if (LockWheel)
	{
		LockWheel->SetRelativeRotation(FRotator(0.0f, 0.0f, Eased * LockWheelRotationDegrees));
	}

	const bool bMoving = !FMath::IsNearlyZero(TravelDirection);
	FLinearColor StatusColor = LockedColor;
	if (!bLocked)
	{
		StatusColor = bMoving ? MovingColor : FMath::Lerp(MovingColor, OpenColor, Eased);
	}

	// Blink while travelling, hold otherwise.
	const float Blink = bMoving
		? 0.45f + 0.55f * FMath::Square(FMath::Sin(GetWorld()->GetTimeSeconds() * 7.5f))
		: 1.0f;

	SetEmissiveColor(StatusColor);
	SetEmissiveLevel(Blink);

	if (ThresholdLight)
	{
		ThresholdLight->SetLightColor(StatusColor);
		ThresholdLight->SetIntensity(ThresholdLightIntensity * Blink * (bLocked ? 0.35f : FMath::Max(Eased, 0.25f)));
	}
}

void ANightfallBlastDoor::RefreshPrompt()
{
	if (!Interactable)
	{
		return;
	}

	if (bLocked)
	{
		Interactable->Verb = LOCTEXT("DoorVerbLocked", "Locked:");
	}
	else if (OpenAmount >= 1.0f || TravelDirection > 0.0f)
	{
		Interactable->Verb = LOCTEXT("DoorVerbClose", "Close");
	}
	else
	{
		Interactable->Verb = LOCTEXT("DoorVerbOpen", "Open");
	}
}

#undef LOCTEXT_NAMESPACE
