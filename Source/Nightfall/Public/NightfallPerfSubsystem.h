// Copyright Nightfall Relay. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPUProfiler.h"
#include "Subsystems/WorldSubsystem.h"
#include "NightfallPerfSubsystem.generated.h"

/** One named cost, in milliseconds. */
USTRUCT(BlueprintType)
struct NIGHTFALL_API FNightfallPerfRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Perf")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Perf")
	float Milliseconds = 0.0f;
};

/**
 * Everything the performance HUD draws.
 *
 * Three sources, deliberately kept separate:
 *
 *  - Frame timings come from the engine's own thread counters and from the RHI's GPU
 *    frame time history, read through a private cursor so nothing here fights `stat unit`.
 *  - Per-system and GPU pass rows come from the stats system. The groups are enabled with
 *    `-nodisplay`, so we collect the data without the engine drawing its own overlay.
 *    Anything declared against STATGROUP_Nightfall appears automatically, which is how a
 *    Game Feature Plugin gets a HUD row without touching this class.
 *  - Streaming cost is measured directly: how many cells are in flight, how long they have
 *    been in flight, and whether the frames that went long did so while they were.
 *
 * In a Shipping build the stats system is compiled out, so the row lists come back empty
 * and the frame timings carry on working. The slice ships Development, where both work.
 */
UCLASS()
class NIGHTFALL_API UNightfallPerfSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End USubsystem

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

	// --- Frame -----------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetFrameMilliseconds() const { return FrameMs; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetFramesPerSecond() const { return FrameMs > KINDA_SMALL_NUMBER ? 1000.0f / FrameMs : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetGameThreadMilliseconds() const { return GameThreadMs; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetRenderThreadMilliseconds() const { return RenderThreadMs; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetRhiThreadMilliseconds() const { return RhiThreadMs; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetGpuMilliseconds() const { return GpuMs; }

	/** Frame budget implied by the project's target frame rate. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetFrameBudgetMilliseconds() const { return BudgetMs; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	bool IsWithinBudget() const { return FrameMs <= BudgetMs; }

	/** Worst frame seen since the last reset, in milliseconds. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetWorstFrameMilliseconds() const { return WorstFrameMs; }

	/** Fraction of recent frames that met the budget, in the range [0,1]. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetBudgetHitRate() const;

	// --- Rows ------------------------------------------------------------------------

	/** Per-system costs from STATGROUP_Nightfall, most expensive first. */
	const TArray<FNightfallPerfRow>& GetSystemRows() const { return SystemRows; }

	/** GPU pass costs, most expensive first. */
	const TArray<FNightfallPerfRow>& GetGpuRows() const { return GpuRows; }

	// --- Streaming -------------------------------------------------------------------

	/** Streaming levels currently loading or unloading. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	int32 GetPendingStreamingCells() const { return PendingStreamingCells; }

	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	bool IsStreamingBusy() const { return PendingStreamingCells > 0; }

	/** Seconds spent with at least one cell in flight since the last reset. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetStreamingBusySeconds() const { return StreamingBusySeconds; }

	/** Frames over budget by more than the hitch threshold, since the last reset. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	int32 GetHitchCount() const { return HitchCount; }

	/** Of those, how many happened while cells were streaming. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	int32 GetStreamingHitchCount() const { return StreamingHitchCount; }

	/** Longest frame recorded while cells were in flight, in milliseconds. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	float GetWorstStreamingFrameMilliseconds() const { return WorstStreamingFrameMs; }

	/** Clear the accumulated hitch and worst-frame counters. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Perf")
	void ResetCounters();

	/** One line summarising the current frame cost, as the HUD would show it. */
	UFUNCTION(BlueprintPure, Category = "Nightfall|Perf")
	FString BuildSummaryLine() const;

	/** Write the summary to the log. Also available as the Nightfall.PerfReport command. */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Perf")
	void LogSummary() const;

	/**
	 * Write the summary plus the per-system and GPU pass breakdowns. This is the report
	 * that answers "what is the frame actually spending its time on".
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Perf")
	void LogBreakdown() const;

	/**
	 * Census of what is actually in the world: actor counts by class, the interaction
	 * registry size, and the components hanging off the player.
	 *
	 * Counting by class name rather than by a list of known types keeps this decoupled -
	 * a Game Feature Plugin's components show up in the player's component list without
	 * this class knowing they exist. Exposed as Nightfall.WorldReport.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Perf")
	void LogWorldReport() const;

	/**
	 * Take a screenshot after a delay, once streaming, exposure and temporal accumulation
	 * have settled. Capturing frame one shows none of those. Exposed as
	 * Nightfall.CaptureAfter <seconds>.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightfall|Perf")
	void CaptureAfter(float Seconds);

private:
	void UpdateFrameTimings(float DeltaTime);
	void UpdateStreaming(float DeltaTime);
	void UpdateStatRows();

	/** Private cursor into the RHI's GPU frame time ring buffer. */
	FRHIGPUFrameTimeHistory::FState GpuTimeCursor;

	TArray<FNightfallPerfRow> SystemRows;
	TArray<FNightfallPerfRow> GpuRows;

	float FrameMs = 0.0f;
	float GameThreadMs = 0.0f;
	float RenderThreadMs = 0.0f;
	float RhiThreadMs = 0.0f;
	float GpuMs = 0.0f;

	float BudgetMs = 16.667f;
	float HitchThresholdMs = 12.0f;

	float WorstFrameMs = 0.0f;
	float WorstStreamingFrameMs = 0.0f;
	float StreamingBusySeconds = 0.0f;

	int32 PendingStreamingCells = 0;
	int32 HitchCount = 0;
	int32 StreamingHitchCount = 0;

	int32 FramesSampled = 0;
	int32 FramesWithinBudget = 0;

	/** Stat rows are refreshed on a slower cadence than the frame; they are noisy. */
	float StatRefreshCountdown = 0.0f;

	/** Countdown to the next automatic log line while Nightfall.PerfLog is set. */
	float PerfLogCountdown = 0.0f;

	/** Seconds until a scheduled screenshot. Negative when none is pending. */
	float CaptureCountdown = -1.0f;

	/** Console objects registered by this subsystem, released on shutdown. */
	TArray<IConsoleObject*> ConsoleObjects;

	/** Turn on the stat groups this HUD reads. Deferred to the first tick; see the cpp. */
	void EnableStatGroups();

	bool bStatGroupsEnabled = false;
};
