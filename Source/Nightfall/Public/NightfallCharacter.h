// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NightfallConspicuous.h"
#include "NightfallCharacter.generated.h"

class UCameraComponent;
class UInputComponent;
class UNightfallCarryComponent;
class UNightfallCharacterMovementComponent;
class UNightfallInputConfig;
class ULocalFogVolumeComponent;
class UNightfallInteractorComponent;
class USpotLightComponent;
struct FInputActionValue;

/**
 * The player.
 *
 * A first-person pawn with no mesh of any kind - the default skeletal mesh subobject is
 * explicitly not created, so nothing in the project can accidentally depend on one. What
 * the player sees is a camera whose local transform is driven procedurally: bob from
 * ground speed, a spring dip on landing, a roll into strafes, and a push on the field of
 * view while sprinting. That is the entire "feel" budget, and it is all transforms.
 *
 * Registers as a modular gameplay receiver, so Game Feature Plugins can attach components
 * to it without this class knowing they exist.
 */
UCLASS()
class NIGHTFALL_API ANightfallCharacter : public ACharacter, public INightfallConspicuous
{
	GENERATED_BODY()

public:
	explicit ANightfallCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	UCameraComponent* GetFirstPersonCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	UNightfallInteractorComponent* GetInteractor() const { return Interactor; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	UNightfallCarryComponent* GetCarry() const { return Carry; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	UNightfallCharacterMovementComponent* GetNightfallMovement() const;

	/** Tag describing the current movement state, for the HUD and for plugins. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	FGameplayTag GetMovementStateTag() const;

	// --- Free flight -------------------------------------------------------------------

	/**
	 * Enter or leave free flight.
	 *
	 * Flight drops the capsule's collision so the camera can pass through the world, moves
	 * along the full view direction rather than the ground plane, and puts ascend and
	 * descend on the jump and crouch keys. Leaving flight restores collision and hands the
	 * character back to gravity wherever it happens to be.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Player")
	void SetFlyModeEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Player")
	void ToggleFlyMode();

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	bool IsFlying() const { return bFlyModeEnabled; }

	/**
	 * Drive the movement input for a few seconds and report how far the character actually
	 * travelled.
	 *
	 * This goes through Enhanced Input's own injection, so it exercises the real path -
	 * action, binding, handler, movement component - rather than calling the handler
	 * directly. It is the check that catches a control scheme that is wired but inert.
	 * Exposed as Nightfall.TestMove.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Player")
	void RunMovementSelfTest(float Seconds, float Forward, float Right);

	/** Input scale applied to ascend and descend, relative to horizontal flight speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlyVerticalScale = 0.85f;

	// --- Phone light -------------------------------------------------------------------

	/**
	 * The light in the player's hand. It is a phone, not a headlamp: a narrow cone at a few
	 * dozen lumens that reaches about nine metres. Enough to read the ground in front of you
	 * and find a cell in the dark, and nowhere near enough to light a pylon's worth of
	 * ground - which is the point, because the pylons are what the game is about.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Player")
	void SetFlashlightEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Nightfall|Player")
	void ToggleFlashlight();

	UFUNCTION(BlueprintPure, Category = "Nightfall|Player")
	bool IsFlashlightOn() const { return bFlashlightOn; }

	/** Emitter output in lumens. A real phone torch is 10 to 50. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player", meta = (ClampMin = "0.0"))
	float FlashlightIntensity = 45.0f;

	//~ INightfallConspicuous
	/**
	 * How visible the player is making themselves. The phone light is most of it; a live
	 * power cell in your hands is the rest, which is what makes the walk back to a pylon
	 * the exposed part of the run rather than the safe part.
	 */
	virtual float GetConspicuity() const override;
	//~ End INightfallConspicuous

	/** Conspicuity contributed by the phone light while it is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashlightConspicuity = 0.65f;

	/** Conspicuity contributed by carrying something that is still glowing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CarriedGlowConspicuity = 0.55f;

	// --- Dust at the feet ---------------------------------------------------------------

	/** Ground speed in cm/s below which nothing is raised. Walking pace: a walk is quiet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float DustMinSpeed = 420.0f;

	/** Ground speed in cm/s at which the running plume is at full strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "1.0"))
	float DustFullSpeed = 760.0f;

	/**
	 * Plume density at a full sprint. A third of a drone's rotor wash, and deliberately
	 * modest: the camera stands inside this volume, so it reads far heavier than the same
	 * number does on a plume seen from across the field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float DustMaxExtinction = 0.28f;

	/** Plume radius in cm while running, and at the peak of a landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "1.0"))
	float DustRadius = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "1.0"))
	float DustLandingRadius = 340.0f;

	/** Dust picks up quickly under the feet and settles slowly, so the rates differ. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float DustRiseRate = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float DustFallRate = 1.8f;

	/** Impact speed in cm/s that produces a full landing puff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "1.0"))
	float LandingDustSpeedFull = 900.0f;

	/** Peak of a landing puff relative to the running plume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float LandingDustScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float LandingDustDecayRate = 2.2f;

	/** Seconds between reads of what is underfoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust", meta = (ClampMin = "0.0"))
	float DustGroundSampleInterval = 0.1f;

	/** Actor tag marking a surface as loose dust. Placed on the terrain by the level build. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Player|Dust")
	FName DustySurfaceTag = TEXT("NF_Dusty");

	/** Optional per-instance override. When unset the project default config is loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nightfall|Input")
	TObjectPtr<const UNightfallInputConfig> InputConfig;

	// --- Camera feel -----------------------------------------------------------------

	/** Eye height above the capsule centre while standing, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float StandingEyeOffset = 64.0f;

	/** Vertical bob amplitude in cm at full walk speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float BobAmplitude = 2.1f;

	/** Bob cycles per second at full walk speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float BobFrequency = 1.85f;

	/** Degrees of camera roll at full lateral input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float StrafeRollDegrees = 1.15f;

	/** Field of view while walking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float BaseFieldOfView = 96.0f;

	/** Extra field of view added at full sprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float SprintFieldOfViewBoost = 7.0f;

	/** Landing dip per 1000 cm/s of downward impact velocity, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float LandingDipScale = 9.0f;

	/** Deepest a landing may dip the camera, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float LandingDipMax = 14.0f;

	/** Landing spring stiffness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float LandingSpringStiffness = 190.0f;

	/** Landing spring damping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Camera")
	float LandingSpringDamping = 19.0f;

	/** Mouse and stick sensitivity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Input")
	float LookSensitivity = 1.0f;

protected:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpCompleted();
	void Input_SprintStarted();
	void Input_SprintCompleted();
	void Input_CrouchStarted();
	void Input_CrouchCompleted();
	void Input_InteractStarted();
	void Input_InteractCompleted();
	void Input_Drop();
	void Input_ToggleFly();
	void Input_ToggleFlashlight();

private:
	void UpdateCameraFeel(float DeltaSeconds);
	void UpdateGroundDust(float DeltaSeconds);

	/** Dustiness of a floor hit: the surface tag first, then the subsystem for grass. */
	float SampleFloorDustiness(const FHitResult& FloorHit) const;

	const UNightfallInputConfig* ResolveInputConfig() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> CameraAnchor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UNightfallInteractorComponent> Interactor;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UNightfallCarryComponent> Carry;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<ULocalFogVolumeComponent> FootDust;

	/** Resolved once in BeginPlay when InputConfig is unset. */
	UPROPERTY(Transient)
	TObjectPtr<const UNightfallInputConfig> ResolvedInputConfig;

	/** Running phase of the walk bob, in radians. */
	float BobPhase = 0.0f;

	/** Current landing spring offset and its velocity, in cm and cm/s. */
	float LandingDip = 0.0f;
	float LandingDipVelocity = 0.0f;

	/** Smoothed lateral input, used for the strafe roll. */
	float SmoothedStrafe = 0.0f;

	/** Last frame's raw movement input, consumed by the camera feel pass. */
	FVector2D LastMoveInput = FVector2D::ZeroVector;

	/** Ascend and descend are held rather than pressed, so flight reads them each tick. */
	bool bAscendHeld = false;
	bool bDescendHeld = false;

	bool bFlyModeEnabled = false;
	bool bFlashlightOn = false;

	/** Ground under the feet, how dusty it is, and the countdown to the next read. */
	float GroundZ = 0.0f;
	float GroundDustiness = 0.0f;
	float DustSampleTimer = 0.0f;

	/** Eased running plume, the decaying landing puff, and the last density pushed. */
	float DustAlpha = 0.0f;
	float LandingDustPuff = 0.0f;
	float LastAppliedDustExtinction = -1.0f;

	/** Movement self test: seconds left, the input being driven, and where it started. */
	float SelfTestRemaining = 0.0f;
	FVector2D SelfTestInput = FVector2D::ZeroVector;
	FVector SelfTestStart = FVector::ZeroVector;

	/** Console objects registered by the locally controlled character. */
	TArray<IConsoleObject*> ConsoleObjects;
};
