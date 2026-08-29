// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NightfallCharacterMovementComponent.generated.h"

/**
 * Movement with three ground speeds and a deliberately heavy feel.
 *
 * The tuning matters as much as the code: high friction and braking so direction changes
 * bite, a long acceleration ramp so sprinting takes a moment to reach, and low air control
 * so a jump commits. The result reads as mass rather than as a floating camera.
 */
UCLASS()
class NIGHTFALL_API UNightfallCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UNightfallCharacterMovementComponent();

	virtual float GetMaxSpeed() const override;

	/** Ground speed with no modifier held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 420.0f;

	/** Ground speed while sprinting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Movement", meta = (ClampMin = "0.0"))
	float SprintSpeed = 760.0f;

	/** Cruise speed in free flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Movement", meta = (ClampMin = "0.0"))
	float FlySpeed = 1500.0f;

	/** Multiplier applied to FlySpeed while the sprint modifier is held in flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Movement", meta = (ClampMin = "1.0"))
	float FlyBoostMultiplier = 3.5f;

	/** Sprint is refused below this forward input, so you cannot sprint sideways or backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Movement", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float SprintMinimumForwardDot = 0.55f;

	/** Set from input each frame. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Movement")
	void SetSprintHeld(bool bHeld) { bSprintHeld = bHeld; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Movement")
	bool IsSprintHeld() const { return bSprintHeld; }

	/** True when the sprint modifier is held and the movement actually qualifies. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Movement")
	bool IsSprinting() const;

private:
	bool bSprintHeld = false;
};
