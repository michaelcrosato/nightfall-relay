// Copyright Nightfall Relay. All Rights Reserved.

#include "ScannerComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "NightfallGameplayTags.h"
#include "NightfallInputConfig.h"
#include "NightfallInteractableComponent.h"
#include "NightfallInteractionSubsystem.h"
#include "NightfallPhysicsProp.h"
#include "SScannerPanel.h"
#include "SurveyScannerRuntime.h"
#include "SurveyScannerTags.h"
#include "UI/NightfallUISubsystem.h"

#define LOCTEXT_NAMESPACE "SurveyScanner"

UScannerComponent::UScannerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Default to this plugin's own content. Overridable, but the feature ships working.
	ScannerInputConfig = TSoftObjectPtr<UNightfallInputConfig>(
		FSoftObjectPath(TEXT("/SurveyScanner/Input/IC_Scanner.IC_Scanner")));
}

void UScannerComponent::BeginPlay()
{
	Super::BeginPlay();

	// Input may not exist yet: a pawn is possessed and restarted after its components
	// begin play. Try now, and listen for the restart in case it has not happened.
	SetUpInput();

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveRestartedDelegate.AddDynamic(this, &UScannerComponent::HandlePawnRestarted);
	}

	if (UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this))
	{
		Panel = SNew(SScannerPanel).Scanner(this);
		UI->RegisterHudPanel(NightfallTags::UI_Layer_Hud, Panel.ToSharedRef());
	}
}

void UScannerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPings();

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		Pawn->ReceiveRestartedDelegate.RemoveDynamic(this, &UScannerComponent::HandlePawnRestarted);

		// Take the mapping context back out, or the key stays bound after the feature is
		// deactivated.
		if (ResolvedInputConfig && ResolvedInputConfig->MappingContext)
		{
			if (const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			{
				if (UEnhancedInputLocalPlayerSubsystem* Input =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
				{
					Input->RemoveMappingContext(ResolvedInputConfig->MappingContext);
				}
			}
		}
	}

	if (Panel.IsValid())
	{
		if (UNightfallUISubsystem* UI = UNightfallUISubsystem::Get(this))
		{
			UI->UnregisterHudPanel(NightfallTags::UI_Layer_Hud, Panel.ToSharedRef());
		}
		Panel.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UScannerComponent::HandlePawnRestarted(APawn* Pawn)
{
	SetUpInput();
}

void UScannerComponent::SetUpInput()
{
	if (bInputBound)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller)
	{
		return;
	}

	if (!ResolvedInputConfig)
	{
		if (ScannerInputConfig.IsNull())
		{
			UE_LOG(LogSurveyScanner, Error, TEXT("Scanner has no input config assigned."));
			return;
		}

		ResolvedInputConfig = ScannerInputConfig.LoadSynchronous();
		if (!ResolvedInputConfig)
		{
			UE_LOG(LogSurveyScanner, Error, TEXT("Scanner input config '%s' failed to load."),
				*ScannerInputConfig.ToString());
			return;
		}
	}

	// The feature owns its key binding: it pushes its own context rather than occupying a
	// slot the core project reserved for it.
	if (ResolvedInputConfig->MappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()))
		{
			Input->AddMappingContext(ResolvedInputConfig->MappingContext, MappingPriority);
		}
	}

	UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!InputComponent)
	{
		// Not an error: the pawn simply has not been restarted yet, and the delegate will
		// bring us back here when it is.
		return;
	}

	const UInputAction* PulseAction = ResolvedInputConfig->FindAction(SurveyScannerTags::Scanner_Input_Pulse);
	if (!PulseAction)
	{
		return;
	}

	InputComponent->BindAction(PulseAction, ETriggerEvent::Started, this, &UScannerComponent::Pulse);
	bInputBound = true;
}

void UScannerComponent::Pulse()
{
	if (CooldownRemaining > 0.0f)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	UNightfallInteractionSubsystem* Interaction = UNightfallInteractionSubsystem::Get(this);
	if (!Pawn || !Interaction)
	{
		return;
	}

	ClearPings();
	Contacts.Reset();

	// The registry already keeps every interactable in one flat array, so a pulse is one
	// pass over it rather than an overlap query against the world.
	const FVector Origin = Pawn->GetActorLocation();
	const TArray<UNightfallInteractableComponent*> Found =
		Interaction->QueryInteractables(FGameplayTag(), Origin, ScanRadius);

	for (UNightfallInteractableComponent* Interactable : Found)
	{
		if (Contacts.Num() >= MaxContacts)
		{
			break;
		}
		if (!Interactable)
		{
			continue;
		}

		FScannerContact Contact;
		Contact.Source = Interactable;
		Contact.Name = Interactable->DisplayName;
		Contact.bPriority = Interactable->InteractionTags.HasTag(NightfallTags::Interactable_Portable);
		Contacts.Add(MoveTemp(Contact));

		// Carryables get tinted so they can be picked out of the dark by eye, not just
		// read off the panel.
		if (ANightfallPhysicsProp* Prop = Cast<ANightfallPhysicsProp>(Interactable->GetOwner()))
		{
			PingedProps.Emplace(Prop, Prop->GlowColor);
			Prop->SetGlowColor(PingColor);
		}
	}

	RefreshContacts();

	ScanRemaining = ScanDuration;
	CooldownRemaining = Cooldown;
}

void UScannerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_SurveyScanner);

	if (CooldownRemaining > 0.0f)
	{
		CooldownRemaining = FMath::Max(CooldownRemaining - DeltaTime, 0.0f);
	}

	if (ScanRemaining <= 0.0f)
	{
		return;
	}

	ScanRemaining -= DeltaTime;
	if (ScanRemaining <= 0.0f)
	{
		ScanRemaining = 0.0f;
		Contacts.Reset();
		ClearPings();
		return;
	}

	RefreshContacts();
}

void UScannerComponent::RefreshContacts()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	const FVector Origin = Pawn->GetActorLocation();

	// Bearings are relative to where the player is looking, so the readout stays correct
	// as they turn without needing another pulse.
	float ViewYaw = Pawn->GetActorRotation().Yaw;
	if (const AController* Controller = Pawn->GetController())
	{
		ViewYaw = Controller->GetControlRotation().Yaw;
	}

	for (int32 Index = Contacts.Num() - 1; Index >= 0; --Index)
	{
		FScannerContact& Contact = Contacts[Index];
		const UNightfallInteractableComponent* Source = Contact.Source.Get();
		if (!Source)
		{
			// Streamed out mid-scan.
			Contacts.RemoveAt(Index);
			continue;
		}

		const FVector ToTarget = Source->GetComponentLocation() - Origin;
		Contact.Distance = static_cast<float>(ToTarget.Size()) / 100.0f;
		Contact.Name = Source->DisplayName;
		Contact.RelativeBearing = FMath::FindDeltaAngleDegrees(ViewYaw, static_cast<float>(ToTarget.Rotation().Yaw));
	}
}

void UScannerComponent::ClearPings()
{
	for (const TPair<TWeakObjectPtr<ANightfallPhysicsProp>, FLinearColor>& Entry : PingedProps)
	{
		if (ANightfallPhysicsProp* Prop = Entry.Key.Get())
		{
			Prop->SetGlowColor(Entry.Value);
		}
	}
	PingedProps.Reset();
}

float UScannerComponent::GetScanProgress() const
{
	if (ScanDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(1.0f - (ScanRemaining / ScanDuration), 0.0f, 1.0f);
}

#undef LOCTEXT_NAMESPACE
