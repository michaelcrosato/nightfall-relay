// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ScannerComponent.generated.h"

class APawn;
class UNightfallInputConfig;
class UNightfallInteractableComponent;

/** One thing the last pulse found, resolved live so bearings track as the player turns. */
USTRUCT(BlueprintType)
struct SURVEYSCANNERRUNTIME_API FScannerContact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scanner")
	FText Name;

	/** Metres from the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Scanner")
	float Distance = 0.0f;

	/** Degrees from where the player is looking. Negative is left. */
	UPROPERTY(BlueprintReadOnly, Category = "Scanner")
	float RelativeBearing = 0.0f;

	/** True for anything the grid feature would call a live power cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Scanner")
	bool bPriority = false;

	TWeakObjectPtr<UNightfallInteractableComponent> Source;
};

/**
 * A pulse scan, added to the player by this feature.
 *
 * The interesting part is what it does not need. It reads the core interaction registry
 * rather than searching the world, ships and pushes its own Enhanced Input mapping rather
 * than borrowing a slot from the core input config, and puts its readout on the HUD through
 * a layer tag. Removing the feature removes the ability, the key binding and the panel
 * together, and the core project is unchanged throughout.
 */
UCLASS(ClassGroup = (SurveyScanner), meta = (BlueprintSpawnableComponent))
class SURVEYSCANNERRUNTIME_API UScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UScannerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fire a pulse. Refused while cooling down. */
	UFUNCTION(BlueprintCallable, Category = "Scanner")
	void Pulse();

	UFUNCTION(BlueprintPure, Category = "Scanner")
	bool IsScanActive() const { return ScanRemaining > 0.0f; }

	/** Seconds until the scanner can fire again. */
	UFUNCTION(BlueprintPure, Category = "Scanner")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	/** How far the current pulse's wavefront has travelled, in the range [0,1]. */
	UFUNCTION(BlueprintPure, Category = "Scanner")
	float GetScanProgress() const;

	const TArray<FScannerContact>& GetContacts() const { return Contacts; }

	/** Radius the pulse reaches, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scanner", meta = (ClampMin = "100.0"))
	float ScanRadius = 6500.0f;

	/** Seconds contacts stay on the readout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scanner", meta = (ClampMin = "0.5"))
	float ScanDuration = 7.0f;

	/** Seconds between pulses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scanner", meta = (ClampMin = "0.0"))
	float Cooldown = 4.0f;

	/** Most contacts shown at once, nearest first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scanner", meta = (ClampMin = "1"))
	int32 MaxContacts = 6;

	/** Colour a scanned carryable is tinted while the pulse holds it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scanner")
	FLinearColor PingColor = FLinearColor(0.85f, 0.98f, 1.0f);

	/** This feature's own input config, containing its mapping context and action. */
	UPROPERTY(EditDefaultsOnly, Category = "Scanner")
	TSoftObjectPtr<UNightfallInputConfig> ScannerInputConfig;

	/** Priority of the pushed mapping context. Above the core context. */
	UPROPERTY(EditDefaultsOnly, Category = "Scanner")
	int32 MappingPriority = 10;

private:
	UFUNCTION()
	void HandlePawnRestarted(APawn* Pawn);

	/** Push the mapping context and bind the pulse action. Safe to call more than once. */
	void SetUpInput();

	/** Recompute distance and bearing for every live contact. */
	void RefreshContacts();

	/** Restore the glow of everything the pulse tinted. */
	void ClearPings();

	UPROPERTY(Transient)
	TObjectPtr<const UNightfallInputConfig> ResolvedInputConfig;

	TArray<FScannerContact> Contacts;

	/** Props tinted by the current pulse, with the colour to put back. */
	TArray<TPair<TWeakObjectPtr<class ANightfallPhysicsProp>, FLinearColor>> PingedProps;

	TSharedPtr<class SScannerPanel> Panel;

	float ScanRemaining = 0.0f;
	float CooldownRemaining = 0.0f;

	bool bInputBound = false;
};
