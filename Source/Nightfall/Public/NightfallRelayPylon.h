// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NightfallMachine.h"
#include "NightfallRelayPylon.generated.h"

class UNightfallInteractableComponent;
class UPointLightComponent;
class USpotLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNightfallPylonChargeSignature, class ANightfallRelayPylon*, Pylon, float, ChargeLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNightfallPylonOnlineSignature, class ANightfallRelayPylon*, Pylon);

/**
 * The mechanical fixture at the centre of the slice: a relay pylon.
 *
 * Six rigid parts and no animation at all. Charge drives everything: the mast telescopes
 * up, the two collector rings spin in opposite directions, the collar creeps round, the
 * core emissive ramps, and two shadow-casting lights come up with it. Bringing one online
 * visibly claims the ground around it, which is the entire reward loop of the slice
 * expressed as lighting rather than as UI.
 *
 * This class knows nothing about where charge comes from. The GridRestoration plugin
 * attaches a component that decides that.
 */
UCLASS()
class NIGHTFALL_API ANightfallRelayPylon : public ANightfallMachine
{
	GENERATED_BODY()

public:
	ANightfallRelayPylon();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnNightfallPostLoad() override;

	// --- Charge ----------------------------------------------------------------------

	/** Set absolute charge in the range [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Pylon")
	void SetChargeLevel(float NewLevel);

	/** Add charge, clamped. Returns the amount actually taken. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Pylon")
	float AddCharge(float Amount);

	UFUNCTION(BlueprintPure, Category = "Nightfall|Pylon")
	float GetChargeLevel() const { return ChargeLevel; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Pylon")
	bool IsOnline() const { return ChargeLevel >= 1.0f - KINDA_SMALL_NUMBER; }

	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Pylon")
	FNightfallPylonChargeSignature OnChargeChanged;

	/** Fires once, the moment the pylon reaches full charge. */
	UPROPERTY(BlueprintAssignable, Category = "Nightfall|Pylon")
	FNightfallPylonOnlineSignature OnCameOnline;

	UFUNCTION(BlueprintPure, Category = "Nightfall|Pylon")
	UNightfallInteractableComponent* GetInteractable() const { return Interactable; }

	// --- Tuning ----------------------------------------------------------------------

	/** How far the mast rises between dormant and online, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon", meta = (ClampMin = "0.0"))
	float MastTravel = 340.0f;

	/** Collector ring spin at full charge, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon")
	float RingSpinSpeed = 155.0f;

	/** Collar yaw rate once online, in degrees per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon")
	float CollarSpinSpeed = 9.0f;

	/** Core light output at full charge, in lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon", meta = (ClampMin = "0.0"))
	float CoreLightIntensity = 8500.0f;

	/** Ground wash output at full charge, in lumens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon", meta = (ClampMin = "0.0"))
	float WashLightIntensity = 22000.0f;

	/** Radius the pylon lights, in cm. Also the radius the grid plugin treats as claimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon", meta = (ClampMin = "100.0"))
	float InfluenceRadius = 2600.0f;

	/** Colour while charging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon")
	FLinearColor ChargingColor = FLinearColor(1.0f, 0.42f, 0.06f);

	/** Colour once online. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nightfall|Pylon")
	FLinearColor OnlineColor = FLinearColor(1.0f, 0.68f, 0.24f);

private:
	void RefreshPresentation();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Collar;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mast;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RingLower;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RingUpper;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Core;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPointLightComponent> CoreLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USpotLightComponent> WashLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UNightfallInteractableComponent> Interactable;

	/** Persisted. This is the only pylon state a save needs to restore. */
	UPROPERTY(SaveGame, VisibleInstanceOnly, Category = "Nightfall|Pylon")
	float ChargeLevel = 0.0f;

	/** Mast height at the top of the last frame's interpolation, in cm. */
	float MastHeight = 0.0f;

	/** Accumulated ring angle, in degrees. */
	float RingAngle = 0.0f;

	/** Accumulated collar angle, in degrees. */
	float CollarAngle = 0.0f;

	bool bBroadcastOnline = false;
};
