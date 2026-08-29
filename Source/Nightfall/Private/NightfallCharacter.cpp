// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputActionValue.h"
#include "Nightfall.h"
#include "NightfallCarryComponent.h"
#include "NightfallCharacterMovementComponent.h"
#include "NightfallGameUserSettings.h"
#include "NightfallGameplayTags.h"
#include "NightfallInputConfig.h"
#include "NightfallInteractableComponent.h"
#include "NightfallDustSubsystem.h"
#include "NightfallInteractorComponent.h"
#include "NightfallPhysicsProp.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallStats.h"

ANightfallCharacter::ANightfallCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		// No skeletal mesh anywhere in this project, including the one ACharacter would
		// otherwise create for us.
		.DoNotCreateDefaultSubobject(ACharacter::MeshComponentName)
		.SetDefaultSubobjectClass<UNightfallCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(38.0f, 92.0f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// The anchor carries crouch height; the camera carries the procedural feel. Splitting
	// them keeps the bob from fighting the crouch interpolation.
	CameraAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CameraAnchor"));
	CameraAnchor->SetupAttachment(Capsule);
	CameraAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, StandingEyeOffset));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	Camera->SetupAttachment(CameraAnchor);
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(BaseFieldOfView);

	// A phone held out in front, not a headlamp. Attached to the camera so it points where
	// the player is looking and inherits the walk bob, offset to the right and down so the
	// cone does not sit on the reticle. The output is deliberately two orders below every
	// fixture in the project: 45 lumens reads the ground at your feet and cannot reach far
	// enough to substitute for a pylon, which is what keeps lighting the field the reward.
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensityUnits(ELightUnits::Lumens);
	Flashlight->SetIntensity(FlashlightIntensity);
	Flashlight->SetInnerConeAngle(12.0f);
	Flashlight->SetOuterConeAngle(30.0f);
	Flashlight->SetAttenuationRadius(900.0f);
	Flashlight->SetSourceRadius(2.0f);
	Flashlight->SetSoftSourceRadius(6.0f);
	// A light sitting on the camera casts shadows that fall entirely behind their own
	// casters, so shadowing it buys almost nothing and is the one cost worth not paying.
	Flashlight->SetCastShadows(false);
	// Well under every other fixture here. Volumetric fog is dense at night, and a
	// camera-mounted light at the usual 1.6-3.2 fills the near froxels and washes white.
	Flashlight->SetVolumetricScatteringIntensity(0.6f);
	Flashlight->SetLightColor(FLinearColor(0.86f, 0.91f, 1.0f));
	Flashlight->SetRelativeLocation(FVector(12.0f, 10.0f, -12.0f));
	Flashlight->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));
	Flashlight->SetVisibility(false);

	// Dust at the feet, in the same medium as the drones' rotor wash and at a person's
	// scale. Parented to the capsule and held in world space: the anchor carries the crouch
	// interpolation and the camera carries the bob, and a plume hanging off either of them
	// swims with the walk. Both extinctions start at zero, which the renderer skips, so
	// standing still costs nothing at all.
	FootDust = CreateDefaultSubobject<ULocalFogVolumeComponent>(TEXT("FootDust"));
	FootDust->SetupAttachment(Capsule);
	FootDust->SetUsingAbsoluteLocation(true);
	FootDust->SetUsingAbsoluteRotation(true);
	FootDust->SetUsingAbsoluteScale(true);
	FootDust->RadialFogExtinction = 0.0f;
	FootDust->HeightFogExtinction = 0.0f;
	// Falloff is in normalised volume units: the layer is 100 * radius / falloff cm thick,
	// so 220 cm of radius here is a 79 cm layer - shin to knee.
	FootDust->HeightFogFalloff = 280.0f;
	FootDust->HeightFogOffset = 0.0f;
	// Softer scattering than the drones' wash. This is the volume the camera sits inside,
	// and at the rotor wash's 0.62 the phone light rakes across it and flares it into a
	// glowing cloud across the bottom of the screen - dust you cannot see past rather than
	// dust at your feet.
	FootDust->FogPhaseG = 0.55f;
	FootDust->FogAlbedo = FLinearColor(0.66f, 0.58f, 0.48f);
	// Same band as a drone plume, so the per-view volume cap never discards it.
	FootDust->FogSortPriority = 40;
	FootDust->SetRelativeScale3D(FVector(DustRadius / ULocalFogVolumeComponent::GetBaseVolumeSize()));

	Interactor = CreateDefaultSubobject<UNightfallInteractorComponent>(TEXT("Interactor"));
	Carry = CreateDefaultSubobject<UNightfallCarryComponent>(TEXT("Carry"));
}

UNightfallCharacterMovementComponent* ANightfallCharacter::GetNightfallMovement() const
{
	return Cast<UNightfallCharacterMovementComponent>(GetCharacterMovement());
}

void ANightfallCharacter::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// Opt in to modular gameplay before components initialise, so anything a Game Feature
	// Plugin adds is present by the time BeginPlay runs.
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void ANightfallCharacter::BeginPlay()
{
	Super::BeginPlay();

	ResolvedInputConfig = ResolveInputConfig();

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this, UGameFrameworkComponentManager::NAME_GameActorReady);

	if (IsLocallyControlled())
	{
		ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Nightfall.Fly"),
			TEXT("Nightfall.Fly [0|1] - enter or leave free flight. No argument toggles."),
			FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
			{
				if (Args.Num() > 0)
				{
					SetFlyModeEnabled(FCString::Atoi(*Args[0]) != 0);
				}
				else
				{
					ToggleFlyMode();
				}
			}),
			ECVF_Default));

		ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Nightfall.Flashlight"),
			TEXT("Nightfall.Flashlight [0|1] - the phone light. No argument toggles."),
			FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
			{
				if (Args.Num() > 0)
				{
					SetFlashlightEnabled(FCString::Atoi(*Args[0]) != 0);
				}
				else
				{
					ToggleFlashlight();
				}
			}),
			ECVF_Default));

		ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Nightfall.TestMove"),
			TEXT("Nightfall.TestMove [seconds] [forward] [right] - drive movement input and report the distance travelled."),
			FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
			{
				const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2.0f;
				const float Forward = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.0f;
				const float Right = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;
				RunMovementSelfTest(Seconds, Forward, Right);
			}),
			ECVF_Default));
	}
}

void ANightfallCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (IConsoleObject* Object : ConsoleObjects)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleObjects.Empty();

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

const UNightfallInputConfig* ANightfallCharacter::ResolveInputConfig() const
{
	if (InputConfig)
	{
		return InputConfig;
	}

	const TSoftObjectPtr<UNightfallInputConfig>& Soft = UNightfallRuntimeSettings::Get().DefaultInputConfig;
	if (Soft.IsNull())
	{
		UE_LOG(LogNightfall, Error,
			TEXT("No input config: set Project Settings > Game > Nightfall Runtime > Default Input Config."));
		return nullptr;
	}

	const UNightfallInputConfig* Loaded = Soft.LoadSynchronous();
	if (!Loaded)
	{
		UE_LOG(LogNightfall, Error, TEXT("Default input config '%s' failed to load."), *Soft.ToString());
	}
	return Loaded;
}

void ANightfallCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	const UNightfallInputConfig* Config = ResolvedInputConfig ? ResolvedInputConfig.Get() : ResolveInputConfig();
	if (!Config || !Config->MappingContext)
	{
		UE_LOG(LogNightfall, Warning, TEXT("PawnClientRestart: no mapping context to apply (config %s)."),
			Config ? TEXT("loaded, MappingContext null") : TEXT("null"));
		return;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		UE_LOG(LogNightfall, Warning, TEXT("PawnClientRestart: controller is not a PlayerController; mapping context not applied."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Subsystem)
	{
		UE_LOG(LogNightfall, Warning, TEXT("PawnClientRestart: no EnhancedInput local player subsystem; mapping context not applied."));
		return;
	}

	Subsystem->AddMappingContext(Config->MappingContext, Config->MappingPriority);
	UE_LOG(LogNightfall, Log, TEXT("PawnClientRestart: applied mapping context at priority %d (registered now: %s)."),
		Config->MappingPriority,
		Subsystem->HasMappingContext(Config->MappingContext) ? TEXT("yes") : TEXT("no"));
}

void ANightfallCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	const UNightfallInputConfig* Config = ResolvedInputConfig ? ResolvedInputConfig.Get() : ResolveInputConfig();
	if (!Config)
	{
		return;
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogNightfall, Error, TEXT("Expected an EnhancedInputComponent. Check DefaultInput.ini."));
		return;
	}

	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Move))
	{
		Input->BindAction(Action, ETriggerEvent::Triggered, this, &ThisClass::Input_Move);
		Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_Move);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Look))
	{
		Input->BindAction(Action, ETriggerEvent::Triggered, this, &ThisClass::Input_Look);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Jump))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_JumpStarted);
		Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_JumpCompleted);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Sprint))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_SprintStarted);
		Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_SprintCompleted);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Crouch))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_CrouchStarted);
		Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_CrouchCompleted);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Interact))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_InteractStarted);
		Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::Input_InteractCompleted);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_Drop))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_Drop);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_ToggleFly))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_ToggleFly);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_ToggleFlashlight))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::Input_ToggleFlashlight);
	}
}

// --- Input handlers ------------------------------------------------------------------

void ANightfallCharacter::Input_Move(const FInputActionValue& Value)
{
	LastMoveInput = Value.Get<FVector2D>();

	if (!Controller || LastMoveInput.IsNearlyZero())
	{
		return;
	}

	// On the ground, movement is flattened to the ground plane so looking down does not
	// slow you. In flight the pitch is the whole point: you go where you are looking.
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator MoveRotation = bFlyModeEnabled
		? ControlRotation
		: FRotator(0.0f, ControlRotation.Yaw, 0.0f);

	const FRotationMatrix MoveBasis(MoveRotation);
	AddMovementInput(MoveBasis.GetUnitAxis(EAxis::X), LastMoveInput.Y);
	AddMovementInput(MoveBasis.GetUnitAxis(EAxis::Y), LastMoveInput.X);
}

void ANightfallCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>() * LookSensitivity;

	// Pitch is authored so that pushing the mouse forward looks up, which is the standard
	// and what the mapping now delivers: no negate on the Look axis, because this project
	// turns off legacy input scales and so never gets APlayerController's -2.5 pitch scale
	// that the stock negate exists to cancel. The preference flips it for anyone who wants
	// it the other way, and covers mouse and pad together since both feed this one handler.
	const UNightfallGameUserSettings* Settings = UNightfallGameUserSettings::GetNightfallSettings();
	const float PitchSign = (Settings && Settings->GetInvertLookY()) ? -1.0f : 1.0f;

	AddControllerYawInput(Look.X);
	AddControllerPitchInput(Look.Y * PitchSign);
}

void ANightfallCharacter::Input_JumpStarted()
{
	bAscendHeld = true;

	if (!bFlyModeEnabled)
	{
		Jump();
	}
}

void ANightfallCharacter::Input_JumpCompleted()
{
	bAscendHeld = false;
	StopJumping();
}

void ANightfallCharacter::Input_SprintStarted()
{
	if (UNightfallCharacterMovementComponent* Movement = GetNightfallMovement())
	{
		Movement->SetSprintHeld(true);
	}

	// Sprinting out of a crouch stands you up rather than being ignored. In flight there
	// is no crouch to leave.
	if (!bFlyModeEnabled)
	{
		UnCrouch();
	}
}

void ANightfallCharacter::Input_SprintCompleted()
{
	if (UNightfallCharacterMovementComponent* Movement = GetNightfallMovement())
	{
		Movement->SetSprintHeld(false);
	}
}

void ANightfallCharacter::Input_CrouchStarted()
{
	bDescendHeld = true;

	if (bFlyModeEnabled)
	{
		return;
	}

	if (UNightfallCharacterMovementComponent* Movement = GetNightfallMovement())
	{
		Movement->SetSprintHeld(false);
	}
	Crouch();
}

void ANightfallCharacter::Input_CrouchCompleted()
{
	bDescendHeld = false;
	UnCrouch();
}

void ANightfallCharacter::Input_InteractStarted()
{
	// A held prop takes priority: interact places it rather than reaching past it.
	if (Carry && Carry->IsCarrying())
	{
		Carry->Release();
		return;
	}

	if (Interactor)
	{
		Interactor->BeginInteract();
	}
}

void ANightfallCharacter::Input_InteractCompleted()
{
	if (Interactor)
	{
		Interactor->EndInteract();
	}
}

void ANightfallCharacter::Input_ToggleFly()
{
	ToggleFlyMode();
}

void ANightfallCharacter::Input_Drop()
{
	if (Carry && Carry->IsCarrying())
	{
		Carry->Throw();
	}
}

void ANightfallCharacter::Input_ToggleFlashlight()
{
	ToggleFlashlight();
}

void ANightfallCharacter::ToggleFlashlight()
{
	SetFlashlightEnabled(!bFlashlightOn);
}

void ANightfallCharacter::SetFlashlightEnabled(bool bEnabled)
{
	if (!Flashlight || bFlashlightOn == bEnabled)
	{
		return;
	}

	bFlashlightOn = bEnabled;
	// Visibility rather than a zero intensity: a light at zero is still registered with the
	// scene, and off is this light's usual state.
	Flashlight->SetIntensity(FlashlightIntensity);
	Flashlight->SetVisibility(bFlashlightOn);

	UE_LOG(LogNightfall, Log, TEXT("Phone light %s."), bFlashlightOn ? TEXT("on") : TEXT("off"));
}

float ANightfallCharacter::GetConspicuity() const
{
	float Conspicuity = bFlashlightOn ? FlashlightConspicuity : 0.0f;

	// A power cell is a light you are holding. Reading its emissive rather than its class
	// means a spent husk stops giving you away the moment it goes dark, and that the core
	// never has to know what a power cell is - only that the thing in your hands glows.
	if (Carry)
	{
		if (const ANightfallPhysicsProp* Carried = Cast<ANightfallPhysicsProp>(Carry->GetCarriedActor()))
		{
			Conspicuity += CarriedGlowConspicuity * FMath::Clamp(Carried->GetEmissiveLevel(), 0.0f, 1.0f);
		}
	}

	return FMath::Clamp(Conspicuity, 0.0f, 1.0f);
}

void ANightfallCharacter::RunMovementSelfTest(float Seconds, float Forward, float Right)
{
	SelfTestRemaining = FMath::Max(Seconds, 0.1f);
	SelfTestInput = FVector2D(Right, Forward);
	SelfTestStart = GetActorLocation();

	UE_LOG(LogNightfall, Log, TEXT("Movement self test: driving (right %.2f, forward %.2f) for %.1fs from (%.0f, %.0f, %.0f)."),
		SelfTestInput.X, SelfTestInput.Y, SelfTestRemaining,
		SelfTestStart.X, SelfTestStart.Y, SelfTestStart.Z);
}

// --- Free flight ---------------------------------------------------------------------

void ANightfallCharacter::ToggleFlyMode()
{
	SetFlyModeEnabled(!bFlyModeEnabled);
}

void ANightfallCharacter::SetFlyModeEnabled(bool bEnabled)
{
	if (bFlyModeEnabled == bEnabled)
	{
		return;
	}

	UNightfallCharacterMovementComponent* Movement = GetNightfallMovement();
	if (!Movement)
	{
		return;
	}

	bFlyModeEnabled = bEnabled;

	if (bFlyModeEnabled)
	{
		// A fly camera that bumps into the world is not a fly camera. Collision goes off
		// so it can pass through geometry, and comes back on when flight ends.
		UnCrouch();
		SetActorEnableCollision(false);
		Movement->SetMovementMode(MOVE_Flying);

		// Nothing is touching the ground any more, so drop whatever was hanging there
		// rather than leaving a plume parked under a camera that has flown away.
		DustAlpha = 0.0f;
		LandingDustPuff = 0.0f;
	}
	else
	{
		SetActorEnableCollision(true);
		// Hand it back to gravity wherever it is rather than snapping to the ground; the
		// landing spring then does its usual job.
		Movement->SetMovementMode(MOVE_Falling);
	}

	// Ascend and descend are held states; leaving flight with one down would otherwise
	// carry a phantom press into the next time flight is entered.
	bAscendHeld = false;
	bDescendHeld = false;

	// Settle the camera rather than carrying a half-finished bob or dip into the change.
	LandingDip = 0.0f;
	LandingDipVelocity = 0.0f;
	BobPhase = 0.0f;

	UE_LOG(LogNightfall, Log, TEXT("Free flight %s."), bFlyModeEnabled ? TEXT("on") : TEXT("off"));
}

// --- Camera feel ---------------------------------------------------------------------

void ANightfallCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float ImpactSpeed = FMath::Abs(static_cast<float>(GetVelocity().Z));
	const float Dip = FMath::Min(ImpactSpeed / 1000.0f * LandingDipScale, LandingDipMax);
	LandingDipVelocity -= Dip * LandingSpringStiffness * 0.05f;

	// The floor is known exactly here, so seed the sample rather than waiting up to a tenth
	// of a second for the throttled read to notice a landing has happened.
	GroundDustiness = SampleFloorDustiness(Hit);
	if (Hit.bBlockingHit)
	{
		GroundZ = Hit.ImpactPoint.Z;
	}

	// A landing hits the ground far harder than a footfall does.
	const float Puff = FMath::Clamp(ImpactSpeed / LandingDustSpeedFull, 0.0f, 1.0f)
		* LandingDustScale * GroundDustiness;
	LandingDustPuff = FMath::Max(LandingDustPuff, Puff);
}

float ANightfallCharacter::SampleFloorDustiness(const FHitResult& FloorHit) const
{
	if (!FloorHit.bBlockingHit)
	{
		return 0.0f;
	}

	// Terrain carries the dusty tag; the compound platform and the structures do not.
	const AActor* HitActor = FloorHit.GetActor();
	float Dustiness = (HitActor && HitActor->ActorHasTag(DustySurfaceTag)) ? 1.0f : 0.0f;

	// Grass has no collision, so the beds have to be asked rather than hit.
	if (const UNightfallDustSubsystem* Dust = UNightfallDustSubsystem::Get(this))
	{
		Dustiness = FMath::Min(Dustiness, Dust->GetGroundDustiness(FloorHit.ImpactPoint));
	}

	return Dustiness;
}

void ANightfallCharacter::UpdateGroundDust(float DeltaSeconds)
{
	const UNightfallCharacterMovementComponent* Movement = GetNightfallMovement();
	if (!FootDust || !Movement)
	{
		return;
	}

	// The movement component has already solved the floor this frame, so what is underfoot
	// costs a struct read rather than a trace. CurrentFloor is only maintained while
	// walking, which is exactly when it matters; the landing hit covers the frame it is not.
	DustSampleTimer -= DeltaSeconds;
	if (DustSampleTimer <= 0.0f && Movement->IsMovingOnGround())
	{
		DustSampleTimer = DustGroundSampleInterval;

		const FFindFloorResult& Floor = Movement->CurrentFloor;
		if (Floor.IsWalkableFloor())
		{
			GroundDustiness = SampleFloorDustiness(Floor.HitResult);
			GroundZ = Floor.HitResult.ImpactPoint.Z;
		}
	}

	// Nothing while still, crouched, airborne, in free flight, or over clean ground. Gated
	// on raw speed rather than IsSprinting(), which only counts forward motion and would
	// let a fast strafe raise nothing.
	const bool bOnFoot = Movement->IsMovingOnGround() && !bFlyModeEnabled && !Movement->IsCrouching();
	const float GroundSpeed = bOnFoot ? Movement->Velocity.Size2D() : 0.0f;
	const float SpeedAlpha = FMath::GetMappedRangeValueClamped(
		FVector2f(DustMinSpeed, DustFullSpeed), FVector2f(0.0f, 1.0f), GroundSpeed);

	const float Target = bOnFoot ? SpeedAlpha * GroundDustiness : 0.0f;
	const float Rate = (Target > DustAlpha) ? DustRiseRate : DustFallRate;
	DustAlpha = FMath::FInterpTo(DustAlpha, Target, DeltaSeconds, Rate);
	LandingDustPuff = FMath::FInterpTo(LandingDustPuff, 0.0f, DeltaSeconds, LandingDustDecayRate);

	const float Total = FMath::Clamp(DustAlpha + LandingDustPuff, 0.0f, LandingDustScale);
	const float Extinction = Total * DustMaxExtinction;

	// Settled and already at zero: leave the volume alone entirely.
	if (Total <= KINDA_SMALL_NUMBER && LastAppliedDustExtinction <= 0.0f)
	{
		return;
	}

	// Sat on the floor under the capsule in world space, so it never inherits the capsule's
	// yaw or the crouch height. Transforms are one render command and effectively free.
	const FVector Location = GetActorLocation();
	FootDust->SetWorldLocation(FVector(Location.X, Location.Y, GroundZ + 20.0f));

	const float PuffAlpha = FMath::Clamp(LandingDustPuff / FMath::Max(LandingDustScale, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float Radius = FMath::Lerp(DustRadius, DustLandingRadius, PuffAlpha);
	FootDust->SetWorldScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));

	// Setting a density marks the render state dirty and rebuilds the scene proxy, so it
	// only moves when it has actually changed.
	if (!FMath::IsNearlyEqual(Extinction, LastAppliedDustExtinction, 0.02f))
	{
		LastAppliedDustExtinction = Extinction;
		FootDust->SetHeightFogExtinction(Extinction);
		FootDust->SetRadialFogExtinction(Extinction * 0.3f);
	}
}

void ANightfallCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Player);

	if (SelfTestRemaining > 0.0f)
	{
		// Inject through Enhanced Input rather than calling the handler, so the mapping
		// context, the action binding and the movement component are all under test.
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			const UNightfallInputConfig* Config = ResolvedInputConfig ? ResolvedInputConfig.Get() : nullptr;
			const UInputAction* MoveAction = Config ? Config->FindAction(NightfallTags::Input_Move, false) : nullptr;

			if (UEnhancedInputLocalPlayerSubsystem* Input =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (MoveAction)
				{
					Input->InjectInputForAction(MoveAction, FInputActionValue(SelfTestInput), {}, {});
				}
			}
		}

		SelfTestRemaining -= DeltaSeconds;
		if (SelfTestRemaining <= 0.0f)
		{
			const FVector Travelled = GetActorLocation() - SelfTestStart;
			UE_LOG(LogNightfall, Log,
				TEXT("Movement self test: travelled %.0f cm (%.0f, %.0f, %.0f), speed now %.0f cm/s."),
				Travelled.Size(), Travelled.X, Travelled.Y, Travelled.Z, GetVelocity().Size());
		}
	}

	// Ascend and descend are held, not pressed, so they are applied here rather than in
	// the input handlers.
	if (bFlyModeEnabled)
	{
		const float Vertical = (bAscendHeld ? 1.0f : 0.0f) - (bDescendHeld ? 1.0f : 0.0f);
		if (!FMath::IsNearlyZero(Vertical))
		{
			AddMovementInput(FVector::UpVector, Vertical * FlyVerticalScale);
		}
	}

	UpdateCameraFeel(DeltaSeconds);
	UpdateGroundDust(DeltaSeconds);
}

void ANightfallCharacter::UpdateCameraFeel(float DeltaSeconds)
{
	const UNightfallCharacterMovementComponent* Movement = GetNightfallMovement();
	if (!Movement || !Camera || !CameraAnchor)
	{
		return;
	}

	// --- Crouch height. Track the capsule so the eye follows the body exactly. --------
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float StandingHalfHeight = GetDefault<ANightfallCharacter>(GetClass())->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float EyeOffset = StandingEyeOffset - (StandingHalfHeight - HalfHeight);
	CameraAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, EyeOffset));

	// --- Walk bob. Amplitude and rate both scale with ground speed. -------------------
	// Flight has no footfalls, so it has no bob.
	const float GroundSpeed = (Movement->IsMovingOnGround() && !bFlyModeEnabled)
		? Movement->Velocity.Size2D()
		: 0.0f;
	const float SpeedRatio = (Movement->WalkSpeed > KINDA_SMALL_NUMBER)
		? FMath::Clamp(GroundSpeed / Movement->WalkSpeed, 0.0f, 2.0f)
		: 0.0f;

	BobPhase += DeltaSeconds * BobFrequency * UE_TWO_PI * SpeedRatio;
	BobPhase = FMath::Fmod(BobPhase, UE_TWO_PI);

	// Vertical bobs at twice the rate of the lateral sway - one dip per footfall, one
	// sway per stride.
	const float BobVertical = FMath::Sin(BobPhase * 2.0f) * BobAmplitude * SpeedRatio;
	const float BobLateral = FMath::Sin(BobPhase) * BobAmplitude * 0.55f * SpeedRatio;

	// --- Landing spring. Critically damped enough to settle in about a third of a second.
	const float SpringAccel = (-LandingDip * LandingSpringStiffness) - (LandingDipVelocity * LandingSpringDamping);
	LandingDipVelocity += SpringAccel * DeltaSeconds;
	LandingDip += LandingDipVelocity * DeltaSeconds;
	LandingDip = FMath::Clamp(LandingDip, -LandingDipMax, LandingDipMax);

	Camera->SetRelativeLocation(FVector(0.0f, BobLateral, BobVertical + LandingDip));

	// --- Strafe roll. Smoothed so a tap does not snap the horizon. --------------------
	SmoothedStrafe = FMath::FInterpTo(SmoothedStrafe, LastMoveInput.X, DeltaSeconds, 6.0f);
	Camera->SetRelativeRotation(FRotator(0.0f, 0.0f, -SmoothedStrafe * StrafeRollDegrees));

	// --- Sprint field of view. ---------------------------------------------------------
	// Boosting in flight gets the same field of view push as sprinting on foot.
	const bool bPushingFov = bFlyModeEnabled
		? (Movement->IsSprintHeld() && !Movement->Velocity.IsNearlyZero())
		: Movement->IsSprinting();
	const float TargetFov = BaseFieldOfView + (bPushingFov ? SprintFieldOfViewBoost : 0.0f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFov, DeltaSeconds, 7.0f));
}

FGameplayTag ANightfallCharacter::GetMovementStateTag() const
{
	const UNightfallCharacterMovementComponent* Movement = GetNightfallMovement();
	if (!Movement)
	{
		return NightfallTags::Player_Movement_Walking;
	}

	if (bFlyModeEnabled)
	{
		return NightfallTags::Player_Movement_Flying;
	}
	if (!Movement->IsMovingOnGround())
	{
		return NightfallTags::Player_Movement_Airborne;
	}
	if (Movement->IsCrouching())
	{
		return NightfallTags::Player_Movement_Crouching;
	}
	if (Movement->IsSprinting())
	{
		return NightfallTags::Player_Movement_Sprinting;
	}
	return NightfallTags::Player_Movement_Walking;
}
