// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "NightfallCarryComponent.generated.h"

class UPhysicsHandleComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightfallCarryChangedSignature, AActor*, CarriedActor);

/**
 * Carries a rigid body in front of the view on a physics handle.
 *
 * The prop stays a simulating Chaos body the whole time, so it collides with the world
 * while carried, swings on the way round corners, and drops naturally. That is the point:
 * with no animation in the project, physics is where the tactility has to come from.
 */
UCLASS(ClassGroup = (Nightfall), meta = (BlueprintSpawnableComponent))
class NIGHTFALL_API UNightfallCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNightfallCarryComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Distance in cm the carried body floats ahead of the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Carry", meta = (ClampMin = "20.0"))
	float CarryDistance = 165.0f;

	/** Bodies heavier than this in kg refuse to be picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Carry", meta = (ClampMin = "0.0"))
	float MaxCarryMassKg = 140.0f;

	/** The carry breaks if the body is dragged further than this from its target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Carry", meta = (ClampMin = "10.0"))
	float BreakDistance = 260.0f;

	/** Impulse applied along the view when throwing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulse = 52000.0f;

	/** Grab a simulating primitive. Returns false if it is too heavy or not simulating. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Carry")
	bool TryCarry(UPrimitiveComponent* Component);

	/** Release whatever is held, leaving it with its current momentum. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Carry")
	void Release();

	/** Release with a shove along the view direction. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Carry")
	void Throw();

	UFUNCTION(BlueprintPure, Category = "Nightfall|Carry")
	bool IsCarrying() const;

	UFUNCTION(BlueprintPure, Category = "Nightfall|Carry")
	AActor* GetCarriedActor() const;

	/** Fires on pickup and on release, with null on release. */
	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Carry")
	FNightfallCarryChangedSignature OnCarryChanged;

private:
	bool GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;
};
