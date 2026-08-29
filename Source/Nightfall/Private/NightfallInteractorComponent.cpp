// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallInteractorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NightfallInteractableComponent.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallStats.h"

UNightfallInteractorComponent::UNightfallInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UNightfallInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Interaction);

	UpdateFocus();

	if (!bInputHeld)
	{
		return;
	}

	UNightfallInteractableComponent* Current = Focus.Get();
	if (!Current || !Current->CanInteract(GetOwner()))
	{
		HoldElapsed = 0.0f;
		return;
	}

	if (Current->HoldSeconds <= 0.0f)
	{
		return;
	}

	HoldElapsed += DeltaTime;
	if (HoldElapsed >= Current->HoldSeconds)
	{
		CompleteInteract();
	}
}

void UNightfallInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetFocus(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UNightfallInteractorComponent::UpdateFocus()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		SetFocus(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const UNightfallRuntimeSettings& Settings = UNightfallRuntimeSettings::Get();
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Settings.InteractionTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NightfallInteractionTrace), /*bTraceComplex=*/false, GetOwner());
	Params.bReturnPhysicalMaterial = false;

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(Settings.InteractionTraceRadius),
		Params);

	if (!bHit || !Hit.GetActor())
	{
		SetFocus(nullptr);
		return;
	}

	// The interactable may sit anywhere on the actor, so search the whole actor rather
	// than only the component the sweep happened to strike.
	UNightfallInteractableComponent* Candidate = Hit.GetActor()->FindComponentByClass<UNightfallInteractableComponent>();
	if (!Candidate)
	{
		SetFocus(nullptr);
		return;
	}

	// Respect a per-instance reach that is shorter than the global trace length.
	const double DistanceSquared = FVector::DistSquared(ViewLocation, Candidate->GetComponentLocation());
	const float Reach = Candidate->GetEffectiveInteractionDistance();
	if (DistanceSquared > static_cast<double>(Reach) * Reach)
	{
		SetFocus(nullptr);
		return;
	}

	SetFocus(Candidate);
}

void UNightfallInteractorComponent::SetFocus(UNightfallInteractableComponent* NewFocus)
{
	UNightfallInteractableComponent* Current = Focus.Get();
	if (Current == NewFocus)
	{
		return;
	}

	if (Current)
	{
		Current->SetFocused(false);
	}

	Focus = NewFocus;
	HoldElapsed = 0.0f;

	if (NewFocus)
	{
		NewFocus->SetFocused(true);
	}

	OnFocusChanged.Broadcast(NewFocus);
}

void UNightfallInteractorComponent::BeginInteract()
{
	bInputHeld = true;
	HoldElapsed = 0.0f;

	UNightfallInteractableComponent* Current = Focus.Get();
	if (Current && Current->HoldSeconds <= 0.0f)
	{
		CompleteInteract();
	}
}

void UNightfallInteractorComponent::EndInteract()
{
	bInputHeld = false;
	HoldElapsed = 0.0f;
}

void UNightfallInteractorComponent::CompleteInteract()
{
	HoldElapsed = 0.0f;

	if (UNightfallInteractableComponent* Current = Focus.Get())
	{
		Current->TryInteract(GetOwner());
	}
}

float UNightfallInteractorComponent::GetHoldProgress() const
{
	const UNightfallInteractableComponent* Current = Focus.Get();
	if (!bInputHeld || !Current || Current->HoldSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(HoldElapsed / Current->HoldSeconds, 0.0f, 1.0f);
}
