// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallCarryComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

UNightfallCarryComponent::UNightfallCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UNightfallCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	// The handle is created here rather than as a default subobject so that this component
	// can be injected onto a pawn by a Game Feature Plugin and still work.
	PhysicsHandle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("NightfallCarryHandle"));
	PhysicsHandle->RegisterComponent();

	// Stiff enough to feel carried, damped enough not to oscillate on stairs.
	PhysicsHandle->bSoftLinearConstraint = true;
	PhysicsHandle->bSoftAngularConstraint = true;
	PhysicsHandle->LinearStiffness = 1400.0f;
	PhysicsHandle->LinearDamping = 190.0f;
	PhysicsHandle->AngularStiffness = 900.0f;
	PhysicsHandle->AngularDamping = 130.0f;
	PhysicsHandle->InterpolationSpeed = 28.0f;
	PhysicsHandle->bInterpolateTarget = true;
}

void UNightfallCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Release();

	if (PhysicsHandle)
	{
		PhysicsHandle->DestroyComponent();
		PhysicsHandle = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool UNightfallCarryComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return false;
	}

	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool UNightfallCarryComponent::TryCarry(UPrimitiveComponent* Component)
{
	if (!PhysicsHandle || !Component || !Component->IsSimulatingPhysics())
	{
		return false;
	}

	if (Component->GetMass() > MaxCarryMassKg)
	{
		return false;
	}

	Release();

	PhysicsHandle->GrabComponentAtLocationWithRotation(
		Component,
		NAME_None,
		Component->GetComponentLocation(),
		Component->GetComponentRotation());

	OnCarryChanged.Broadcast(Component->GetOwner());
	return true;
}

void UNightfallCarryComponent::Release()
{
	if (!PhysicsHandle || !PhysicsHandle->GetGrabbedComponent())
	{
		return;
	}

	PhysicsHandle->ReleaseComponent();
	OnCarryChanged.Broadcast(nullptr);
}

void UNightfallCarryComponent::Throw()
{
	if (!PhysicsHandle)
	{
		return;
	}

	UPrimitiveComponent* Grabbed = PhysicsHandle->GetGrabbedComponent();
	if (!Grabbed)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	const bool bHasView = GetViewPoint(ViewLocation, ViewRotation);

	PhysicsHandle->ReleaseComponent();

	if (bHasView)
	{
		Grabbed->AddImpulse(ViewRotation.Vector() * ThrowImpulse);
	}

	OnCarryChanged.Broadcast(nullptr);
}

bool UNightfallCarryComponent::IsCarrying() const
{
	return PhysicsHandle && PhysicsHandle->GetGrabbedComponent() != nullptr;
}

AActor* UNightfallCarryComponent::GetCarriedActor() const
{
	if (!PhysicsHandle)
	{
		return nullptr;
	}

	const UPrimitiveComponent* Grabbed = PhysicsHandle->GetGrabbedComponent();
	return Grabbed ? Grabbed->GetOwner() : nullptr;
}

void UNightfallCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsCarrying())
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (!GetViewPoint(ViewLocation, ViewRotation))
	{
		Release();
		return;
	}

	const FVector Target = ViewLocation + ViewRotation.Vector() * CarryDistance;

	// If the world has pushed the body far enough from where it should be, the player has
	// dragged it into geometry. Let go rather than forcing it through.
	const UPrimitiveComponent* Grabbed = PhysicsHandle->GetGrabbedComponent();
	if (FVector::Dist(Grabbed->GetComponentLocation(), Target) > BreakDistance)
	{
		Release();
		return;
	}

	PhysicsHandle->SetTargetLocationAndRotation(Target, ViewRotation);
}
