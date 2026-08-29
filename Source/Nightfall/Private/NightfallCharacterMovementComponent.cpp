// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallCharacterMovementComponent.h"

#include "GameFramework/Character.h"

UNightfallCharacterMovementComponent::UNightfallCharacterMovementComponent()
{
	// Ground handling: firm, with a noticeable ramp on and off.
	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = 195.0f;
	MaxAcceleration = 1650.0f;
	BrakingDecelerationWalking = 1900.0f;
	BrakingFrictionFactor = 1.0f;
	GroundFriction = 7.5f;
	bUseSeparateBrakingFriction = true;
	BrakingFriction = 5.0f;

	// Air: a jump commits. Enough control to correct a landing, not enough to steer.
	JumpZVelocity = 520.0f;
	AirControl = 0.22f;
	AirControlBoostMultiplier = 1.6f;
	AirControlBoostVelocityThreshold = 25.0f;
	BrakingDecelerationFalling = 120.0f;
	FallingLateralFriction = 0.35f;
	GravityScale = 1.15f;

	// Crouch.
	NavAgentProps.bCanCrouch = true;
	bCanWalkOffLedgesWhenCrouching = true;
	SetCrouchedHalfHeight(56.0f);

	// Steps and slopes suited to catwalks and rubble.
	MaxStepHeight = 42.0f;
	SetWalkableFloorAngle(50.0f);
	PerchRadiusThreshold = 12.0f;
	LedgeCheckThreshold = 4.0f;

	// Push rigid bodies around rather than sliding through them.
	bEnablePhysicsInteraction = true;
	StandingDownwardForceScale = 1.0f;
	InitialPushForceFactor = 500.0f;
	PushForceFactor = 750000.0f;
	bPushForceScaledToMass = true;
	bScalePushForceToVelocity = true;

	// Free flight. Braking is high so the camera stops where you let go of the stick,
	// which is what makes a fly camera usable for looking at things.
	MaxFlySpeed = FlySpeed;
	BrakingDecelerationFlying = 5200.0f;

	bUseControllerDesiredRotation = false;
	bOrientRotationToMovement = false;
}

bool UNightfallCharacterMovementComponent::IsSprinting() const
{
	if (!bSprintHeld || !IsMovingOnGround() || IsCrouching())
	{
		return false;
	}

	// Sprinting is a forward commitment. Strafing at sprint speed feels wrong and lets
	// players circle-strafe drones far too easily.
	const FVector Velocity2D = Velocity.GetSafeNormal2D();
	if (Velocity2D.IsNearlyZero())
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(Forward, Velocity2D) >= SprintMinimumForwardDot;
}

float UNightfallCharacterMovementComponent::GetMaxSpeed() const
{
	if (MovementMode == MOVE_Flying)
	{
		// Sprint is a boost in the air rather than a separate gait, and the forward-only
		// rule does not apply: a fly camera should accelerate whichever way it is pointed.
		return bSprintHeld ? FlySpeed * FlyBoostMultiplier : FlySpeed;
	}

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		if (IsCrouching())
		{
			return MaxWalkSpeedCrouched;
		}
		return IsSprinting() ? SprintSpeed : WalkSpeed;
	}

	return Super::GetMaxSpeed();
}
