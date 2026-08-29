// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/NightfallUISubsystem.h"

#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Nightfall.h"
#include "NightfallGameplayTags.h"
#include "NightfallInputConfig.h"
#include "NightfallPlayerController.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallWorldClockSubsystem.h"
#include "UI/SNightfallBriefing.h"
#include "UI/SNightfallHudRoot.h"
#include "UI/SNightfallSettingsMenu.h"

namespace
{
	/** Viewport z-order for the HUD. */
	constexpr int32 HudZOrder = 10;

	/** Viewport z-order for the settings menu, above the HUD. */
	constexpr int32 MenuZOrder = 40;

	/**
	 * Viewport z-order for the briefing: over the HUD it replaces, under the settings menu
	 * so opening settings from the console never buries itself behind the card.
	 */
	constexpr int32 BriefingZOrder = 30;
}

UNightfallUISubsystem* UNightfallUISubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	const ULocalPlayer* LocalPlayer = World ? World->GetFirstLocalPlayerFromController() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UNightfallUISubsystem>() : nullptr;
}

void UNightfallUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The menu is normally reached with the menu key. Having it on the console as well
	// means it can be opened from a command line, which is how it gets captured.
	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.Settings"),
		TEXT("Open or close the settings menu."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { ToggleSettingsMenu(); }),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.PerfHud"),
		TEXT("Cycle the performance HUD: hidden, compact, full."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { CyclePerfHud(); }),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.Briefing"),
		TEXT("Show or dismiss the briefing card."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { ToggleBriefing(); }),
		ECVF_Default));
}

void UNightfallUISubsystem::Deinitialize()
{
	for (IConsoleObject* Object : ConsoleObjects)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleObjects.Empty();

	CloseSettingsMenu();
	DismissBriefing();

	if (HudRoot.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(HudRoot.ToSharedRef());
	}
	HudRoot.Reset();
	PendingPanels.Empty();

	Super::Deinitialize();
}

void UNightfallUISubsystem::CreateHud(APlayerController* OwningController)
{
	WeakController = OwningController;

	if (HudRoot.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	HudRoot = SNew(SNightfallHudRoot).World(OwningController ? OwningController->GetWorld() : nullptr);
	GEngine->GameViewport->AddViewportWidgetContent(HudRoot.ToSharedRef(), HudZOrder);

	// Anything that registered while the HUD did not exist gets attached now.
	for (const TPair<FGameplayTag, TSharedRef<SWidget>>& Pending : PendingPanels)
	{
		HudRoot->AddPanel(Pending.Key, Pending.Value);
	}
	PendingPanels.Empty();

	if (PendingPerfHudMode.IsSet())
	{
		HudRoot->SetPerfHudMode(PendingPerfHudMode.GetValue());
		PendingPerfHudMode.Reset();
	}
}

void UNightfallUISubsystem::BindInputActions(UInputComponent* InputComponent)
{
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		return;
	}

	const TSoftObjectPtr<UNightfallInputConfig>& Soft = UNightfallRuntimeSettings::Get().DefaultInputConfig;
	const UNightfallInputConfig* Config = Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	if (!Config)
	{
		UE_LOG(LogNightfall, Error, TEXT("UI input not bound: no default input config is configured."));
		return;
	}

	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_ToggleMenu))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &UNightfallUISubsystem::ToggleSettingsMenu);
	}
	if (const UInputAction* Action = Config->FindAction(NightfallTags::Input_TogglePerfHud))
	{
		Input->BindAction(Action, ETriggerEvent::Started, this, &UNightfallUISubsystem::CyclePerfHud);
	}
}

bool UNightfallUISubsystem::RegisterHudPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel)
{
	if (HudRoot.IsValid())
	{
		return HudRoot->AddPanel(Layer, Panel);
	}

	PendingPanels.Emplace(Layer, Panel);
	return true;
}

void UNightfallUISubsystem::UnregisterHudPanel(FGameplayTag Layer, const TSharedRef<SWidget>& Panel)
{
	if (HudRoot.IsValid())
	{
		HudRoot->RemovePanel(Layer, Panel);
	}

	PendingPanels.RemoveAll([&Layer, &Panel](const TPair<FGameplayTag, TSharedRef<SWidget>>& Entry)
	{
		return Entry.Key == Layer && Entry.Value == Panel;
	});
}

void UNightfallUISubsystem::CyclePerfHud()
{
	if (HudRoot.IsValid())
	{
		HudRoot->CyclePerfHudMode();
		return;
	}

	// No HUD yet. Advance from whatever was last asked for and remember it.
	const uint8 Current = static_cast<uint8>(
		PendingPerfHudMode.Get(ENightfallPerfHudMode::Compact));
	PendingPerfHudMode = static_cast<ENightfallPerfHudMode>(
		(Current + 1) % static_cast<uint8>(ENightfallPerfHudMode::Count));
}

void UNightfallUISubsystem::ToggleSettingsMenu()
{
	// While the card is up the menu key is what takes it down. Without this, the first thing
	// a new player presses would stack the settings menu on top of a briefing they have not
	// finished reading.
	if (Briefing.IsValid())
	{
		DismissBriefing();
		return;
	}

	if (SettingsMenu.IsValid())
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

void UNightfallUISubsystem::OpenSettingsMenu()
{
	if (SettingsMenu.IsValid() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	SettingsMenu = SNew(SNightfallSettingsMenu)
		.OnCloseRequested(SNightfallSettingsMenu::FOnCloseRequested::CreateUObject(this, &UNightfallUISubsystem::CloseSettingsMenu));

	GEngine->GameViewport->AddViewportWidgetContent(SettingsMenu.ToSharedRef(), MenuZOrder);

	// The menu needs the cursor, and the world should keep running behind it so the
	// player can see a setting take effect while the panel is still open.
	if (ANightfallPlayerController* Controller = Cast<ANightfallPlayerController>(WeakController.Get()))
	{
		Controller->ApplyMenuInputMode();
	}
}

void UNightfallUISubsystem::CloseSettingsMenu()
{
	if (!SettingsMenu.IsValid())
	{
		return;
	}

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(SettingsMenu.ToSharedRef());
	}
	SettingsMenu.Reset();

	if (ANightfallPlayerController* Controller = Cast<ANightfallPlayerController>(WeakController.Get()))
	{
		Controller->ApplyGameInputMode();
	}
}

UNightfallWorldClockSubsystem* UNightfallUISubsystem::GetClock() const
{
	APlayerController* Controller = WeakController.Get();
	UWorld* World = Controller ? Controller->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UNightfallWorldClockSubsystem>() : nullptr;
}

void UNightfallUISubsystem::ShowBriefing()
{
	if (Briefing.IsValid())
	{
		return;
	}

	if (!GEngine || !GEngine->GameViewport)
	{
		UE_LOG(LogNightfall, Warning, TEXT("Briefing not shown: there is no game viewport yet."));
		return;
	}

	Briefing = SNew(SNightfallBriefing)
		.OnDismissRequested(SNightfallBriefing::FOnDismissRequested::CreateUObject(this, &UNightfallUISubsystem::DismissBriefing));

	GEngine->GameViewport->AddViewportWidgetContent(Briefing.ToSharedRef(), BriefingZOrder);

	// Hold the day while the card is up. At the shipped day length one in-game hour costs
	// ninety real seconds, so a player who reads to the end would step out into full night
	// having never seen the dusk the card is describing.
	if (UNightfallWorldClockSubsystem* Clock = GetClock())
	{
		if (!Clock->IsPaused())
		{
			Clock->SetPaused(true);
			bPausedClockForBriefing = true;
		}
	}

	// The input mode is deliberately left alone. The game runs in FInputModeGameOnly, where
	// the menu key still reaches the player controller and reaches DismissBriefing through
	// ToggleSettingsMenu; switching to GameAndUI here would surface a cursor and kill look.
}

void UNightfallUISubsystem::DismissBriefing()
{
	if (!Briefing.IsValid())
	{
		return;
	}

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Briefing.ToSharedRef());
	}
	Briefing.Reset();

	if (bPausedClockForBriefing)
	{
		if (UNightfallWorldClockSubsystem* Clock = GetClock())
		{
			Clock->SetPaused(false);
		}
		bPausedClockForBriefing = false;
	}
}

void UNightfallUISubsystem::ToggleBriefing()
{
	if (Briefing.IsValid())
	{
		DismissBriefing();
	}
	else
	{
		ShowBriefing();
	}
}
