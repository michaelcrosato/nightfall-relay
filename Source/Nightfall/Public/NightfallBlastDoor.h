// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NightfallMachine.h"
#include "NightfallBlastDoor.generated.h"

class UNightfallInteractableComponent;
class URectLightComponent;

/**
 * The second mechanical fixture: a two-leaf blast door with a spinning lock wheel.
 *
 * Included alongside the pylon because it exercises a different shape of motion - a
 * bounded travel with an eased profile and a hard end stop, rather than a continuous
 * spin. The same class covers lifts and shutters by changing the travel axis.
 */
UCLASS()
class NIGHTFALL_API ANightfallBlastDoor : public ANightfallMachine
{
	GENERATED_BODY()

public:
	ANightfallBlastDoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnNightfallPostLoad() override;

	/** Start opening. Refused while locked. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Door")
	void Open();

	/** Start closing. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Door")
	void Close();

	/** Open if closed or closing, close otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Door")
	void Toggle();

	/** Locked doors refuse to open and show a red status panel. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Door")
	void SetLocked(bool bNewLocked);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Door")
	bool IsLocked() const { return bLocked; }

	/** Travel completion in the range [0,1], where 1 is fully open. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Door")
	float GetOpenAmount() const { return OpenAmount; }

	/** How far each leaf slides along its local Y, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door", meta = (ClampMin = "0.0"))
	float LeafTravel = 190.0f;

	/** Seconds for a full open or close. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door", meta = (ClampMin = "0.1"))
	float TravelSeconds = 2.1f;

	/** Degrees the lock wheel turns over a full travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door")
	float LockWheelRotationDegrees = 540.0f;

	/** Seconds to hold open before closing on its own. Zero stays open. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door", meta = (ClampMin = "0.0"))
	float AutoCloseDelay = 8.0f;

	/** Doorway light output in lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door", meta = (ClampMin = "0.0"))
	float ThresholdLightIntensity = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door")
	FLinearColor LockedColor = FLinearColor(1.0f, 0.08f, 0.10f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door")
	FLinearColor MovingColor = FLinearColor(1.0f, 0.55f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Door")
	FLinearColor OpenColor = FLinearColor(0.24f, 1.0f, 0.52f);

private:
	UFUNCTION()
	void HandleInteracted(UNightfallInteractableComponent* Source, AActor* Interactor);

	void RefreshPresentation();
	void RefreshPrompt();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeafLeft;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeafRight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LockWheel;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StatusPanel;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<URectLightComponent> ThresholdLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UNightfallInteractableComponent> Interactable;

	/** Persisted so a save reopens the doors the player left open. */
	UPROPERTY(SaveGame, VisibleInstanceOnly, Category = "Nightfall|Door")
	float OpenAmount = 0.0f;

	UPROPERTY(SaveGame, VisibleInstanceOnly, Category = "Nightfall|Door")
	bool bLocked = false;

	/** +1 opening, -1 closing, 0 at rest. */
	float TravelDirection = 0.0f;

	float AutoCloseRemaining = 0.0f;
};
