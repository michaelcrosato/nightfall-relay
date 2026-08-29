// Copyright Nightfall Relay. All Rights Reserved.

#include "NightfallPerfSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NightfallInteractableComponent.h"
#include "NightfallInteractionSubsystem.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Nightfall.h"
#include "NightfallRuntimeSettings.h"
#include "NightfallStats.h"
#include "RenderTimer.h"
#include "UObject/UObjectIterator.h"

#if STATS
#include "Stats/StatsCommand.h"
#include "Stats/StatsData.h"
#endif

namespace
{
	/** Seconds between stat row refreshes. Fast enough to react, slow enough to read. */
	constexpr float StatRefreshInterval = 0.25f;

	/** Weight applied to the newest sample when smoothing frame timings. */
	constexpr float TimingSmoothing = 0.10f;

	/** Rows below this cost are noise and are dropped from the HUD. */
	constexpr float MinimumInterestingMs = 0.02f;

	/** Cap on rows kept per list, so a HUD panel cannot grow without bound. */
	constexpr int32 MaxRowsPerList = 12;

	float SmoothTowards(float Current, float Sample)
	{
		return (Current <= 0.0f) ? Sample : FMath::Lerp(Current, Sample, TimingSmoothing);
	}

	/**
	 * Seconds between automatic summary lines while logging is on. One a second is enough
	 * to characterise a session without burying the log.
	 */
	constexpr float PerfLogInterval = 1.0f;

	/** Set Nightfall.PerfLog 1 to stream the summary to the log. */
	TAutoConsoleVariable<int32> CVarPerfLog(
		TEXT("Nightfall.PerfLog"),
		0,
		TEXT("When 1, write one performance summary line per second to the log. ")
		TEXT("Useful for checking the frame budget from a headless or windowed run."),
		ECVF_Default);
}

void UNightfallPerfSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNightfallRuntimeSettings& Settings = UNightfallRuntimeSettings::Get();
	BudgetMs = 1000.0f / FMath::Max(Settings.TargetFrameRate, 1.0f);
	HitchThresholdMs = Settings.HitchThresholdMilliseconds;


	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.PerfReport"),
		TEXT("Write the current performance summary to the log."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { LogBreakdown(); }),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.WorldReport"),
		TEXT("Log a census of the actors and components currently in the world."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]() { LogWorldReport(); }),
		ECVF_Default));

	ConsoleObjects.Add(IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Nightfall.CaptureAfter"),
		TEXT("Nightfall.CaptureAfter <seconds> - screenshot once the view has settled."),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			CaptureAfter(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.0f);
		}),
		ECVF_Default));
}

void UNightfallPerfSubsystem::Deinitialize()
{
#if STATS
	if (bStatGroupsEnabled)
	{
		UE::Stats::DirectStatsCommand(TEXT("stat gpu -nodisplay"), /*bBlockForCompletion=*/true);
		UE::Stats::DirectStatsCommand(TEXT("stat nightfall -nodisplay"), /*bBlockForCompletion=*/true);
		bStatGroupsEnabled = false;
	}
#endif

	for (IConsoleObject* Object : ConsoleObjects)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Object);
	}
	ConsoleObjects.Empty();

	Super::Deinitialize();
}

bool UNightfallPerfSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UNightfallPerfSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNightfallPerfSubsystem, STATGROUP_Tickables);
}

void UNightfallPerfSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SCOPE_CYCLE_COUNTER(STAT_Nightfall_Telemetry);

	// Enabling the stat groups is deferred to the first tick rather than done in
	// Initialize: a world subsystem initialises before the stats thread is ready to take
	// commands, and one issued that early is simply dropped, leaving the HUD's row lists
	// permanently empty.
	if (!bStatGroupsEnabled)
	{
		EnableStatGroups();
	}

	UpdateFrameTimings(DeltaTime);
	UpdateStreaming(DeltaTime);

	StatRefreshCountdown -= DeltaTime;
	if (StatRefreshCountdown <= 0.0f)
	{
		StatRefreshCountdown = StatRefreshInterval;
		UpdateStatRows();
	}

	if (CaptureCountdown >= 0.0f)
	{
		CaptureCountdown -= DeltaTime;
		if (CaptureCountdown < 0.0f)
		{
			LogWorldReport();
			LogBreakdown();

			// Request through the screenshot system directly rather than the console
			// command, which needs a viewport client to route it. bShowUI keeps the HUD
			// in frame, which is the point of capturing a settled shot.
			FScreenshotRequest::RequestScreenshot(
				TEXT("NightfallCapture"), /*bInShowUI=*/true, /*bAddFilenameSuffix=*/true);
		}
	}

	if (CVarPerfLog.GetValueOnGameThread() != 0)
	{
		PerfLogCountdown -= DeltaTime;
		if (PerfLogCountdown <= 0.0f)
		{
			PerfLogCountdown = PerfLogInterval;
			LogSummary();
		}
	}
}

FString UNightfallPerfSubsystem::BuildSummaryLine() const
{
	return FString::Printf(
		TEXT("%5.1f fps | frame %5.2f (budget %4.1f) | game %5.2f render %5.2f rhi %5.2f gpu %5.2f")
		TEXT(" | in budget %5.1f%% | worst %5.2f | cells %d | hitches %d (%d streaming)"),
		GetFramesPerSecond(), FrameMs, BudgetMs,
		GameThreadMs, RenderThreadMs, RhiThreadMs, GpuMs,
		GetBudgetHitRate() * 100.0f, WorstFrameMs,
		PendingStreamingCells, HitchCount, StreamingHitchCount);
}

void UNightfallPerfSubsystem::LogSummary() const
{
	UE_LOG(LogNightfall, Log, TEXT("PERF %s"), *BuildSummaryLine());
}

void UNightfallPerfSubsystem::EnableStatGroups()
{
	bStatGroupsEnabled = true;

#if STATS
	// -nodisplay collects the data without the engine drawing its own stat overlay over
	// ours. Both groups are needed: GPU for pass costs, Nightfall for our own systems.
	UE::Stats::DirectStatsCommand(TEXT("stat gpu -nodisplay"), /*bBlockForCompletion=*/true);
	UE::Stats::DirectStatsCommand(TEXT("stat nightfall -nodisplay"), /*bBlockForCompletion=*/true);
#endif
}

void UNightfallPerfSubsystem::UpdateFrameTimings(float DeltaTime)
{
	// Wall clock for the frame. FApp's delta is the number the player actually feels.
	const float RawFrameMs = static_cast<float>(FApp::GetDeltaTime() * 1000.0);

	GameThreadMs = SmoothTowards(GameThreadMs, FPlatformTime::ToMilliseconds(GGameThreadTime));
	RenderThreadMs = SmoothTowards(RenderThreadMs, FPlatformTime::ToMilliseconds(GRenderThreadTime));
	RhiThreadMs = SmoothTowards(RhiThreadMs, FPlatformTime::ToMilliseconds(GRHIThreadTime));

	// Drain every GPU timing the RHI has produced since the last tick and keep the newest.
	// The cursor is ours alone, so draining it does not starve `stat unit`.
	uint64 GpuCycles = 0;
	uint64 NewestGpuCycles = 0;
	bool bGotGpuTiming = false;
	while (GpuTimeCursor.PopFrameCycles(GpuCycles) != FRHIGPUFrameTimeHistory::EResult::Empty)
	{
		NewestGpuCycles = GpuCycles;
		bGotGpuTiming = true;
	}
	if (bGotGpuTiming)
	{
		GpuMs = SmoothTowards(GpuMs, static_cast<float>(FPlatformTime::ToMilliseconds64(NewestGpuCycles)));
	}

	FrameMs = SmoothTowards(FrameMs, RawFrameMs);

	// Budget accounting runs on the raw value, not the smoothed one: a single long frame
	// is exactly what we want to catch.
	++FramesSampled;
	if (RawFrameMs <= BudgetMs)
	{
		++FramesWithinBudget;
	}

	WorstFrameMs = FMath::Max(WorstFrameMs, RawFrameMs);

	if (RawFrameMs > BudgetMs + HitchThresholdMs)
	{
		++HitchCount;
		if (PendingStreamingCells > 0)
		{
			++StreamingHitchCount;
		}
	}

	if (PendingStreamingCells > 0)
	{
		WorstStreamingFrameMs = FMath::Max(WorstStreamingFrameMs, RawFrameMs);
	}
}

void UNightfallPerfSubsystem::UpdateStreaming(float DeltaTime)
{
	PendingStreamingCells = 0;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// World Partition drives its runtime cells through ordinary streaming levels, so
	// counting those covers both hand-placed sublevels and generated cells.
	for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->IsStreamingStatePending())
		{
			++PendingStreamingCells;
		}
	}

	if (PendingStreamingCells > 0)
	{
		StreamingBusySeconds += DeltaTime;
	}
}

void UNightfallPerfSubsystem::UpdateStatRows()
{
	SystemRows.Reset();
	GpuRows.Reset();

#if STATS
	const FGameThreadStatsData* Data = FLatestGameThreadStatsData::Get().Latest;
	if (!Data)
	{
		return;
	}

	static const FName NightfallGroupName(TEXT("STATGROUP_Nightfall"));

	auto ReadMilliseconds = [](const FComplexStatMessage& Message, float& OutMs) -> bool
	{
		const EStatDataType::Type DataType = Message.NameAndInfo.GetField<EStatDataType>();

		if (DataType == EStatDataType::ST_int64 && Message.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
		{
			OutMs = static_cast<float>(FPlatformTime::ToMilliseconds64(Message.GetValue_Duration(EComplexStatField::IncAve)));
			return true;
		}

		// GPU pass timings arrive as float counters already expressed in milliseconds.
		if (DataType == EStatDataType::ST_double)
		{
			OutMs = static_cast<float>(Message.GetValue_double(EComplexStatField::IncAve));
			return true;
		}

		return false;
	};

	// Raw stat names are namespaced for the stats system, not for reading: a cycle counter
	// comes back as STAT_Nightfall_Machines and a GPU pass as STAT_GPU0_Graphics0_MegaLights.
	// Prefer the authored description, and otherwise strip the machinery.
	auto MakeDisplayName = [](const FComplexStatMessage& Message) -> FName
	{
		FString Name = Message.NameAndInfo.GetShortName().ToString();

		// GPU passes must keep their queue: several are called "Queue Total" and the
		// description alone would collapse graphics and async compute into one row.
		if (!Name.StartsWith(TEXT("STAT_GPU")))
		{
			const FString Description = Message.GetDescription();
			if (!Description.IsEmpty())
			{
				return FName(*Description);
			}
		}

		Name.RemoveFromStart(TEXT("STAT_"));

		// Drop the device prefix but keep the queue, because which queue a pass ran on is
		// exactly the interesting part when async compute is in play.
		if (Name.StartsWith(TEXT("GPU")))
		{
			int32 Separator = INDEX_NONE;
			if (Name.FindChar(TEXT('_'), Separator))
			{
				Name.MidInline(Separator + 1, MAX_int32, EAllowShrinking::No);
			}
		}

		Name.ReplaceInline(TEXT("_"), TEXT(" "));
		return FName(*Name);
	};

	auto Collect = [&ReadMilliseconds, &MakeDisplayName](const TArray<FComplexStatMessage>& Messages, TArray<FNightfallPerfRow>& OutRows)
	{
		for (const FComplexStatMessage& Message : Messages)
		{
			float Milliseconds = 0.0f;
			if (!ReadMilliseconds(Message, Milliseconds) || Milliseconds < MinimumInterestingMs)
			{
				continue;
			}

			FNightfallPerfRow& Row = OutRows.AddDefaulted_GetRef();
			Row.Name = MakeDisplayName(Message);
			Row.Milliseconds = Milliseconds;
		}
	};

	const int32 NumGroups = FMath::Min(Data->GroupNames.Num(), Data->ActiveStatGroups.Num());
	for (int32 GroupIndex = 0; GroupIndex < NumGroups; ++GroupIndex)
	{
		const FName GroupName = Data->GroupNames[GroupIndex];
		const FActiveStatGroupInfo& Group = Data->ActiveStatGroups[GroupIndex];

		if (GroupName == NightfallGroupName)
		{
			Collect(Group.FlatAggregate, SystemRows);
			continue;
		}

		// GPU stat groups are created per queue at runtime, so match on the name rather
		// than on a compile-time group id.
		if (GroupName.ToString().Contains(TEXT("GPU")))
		{
			Collect(Group.FlatAggregate, GpuRows);
			Collect(Group.GpuStatsAggregate, GpuRows);
		}
	}

	auto SortDescending = [](TArray<FNightfallPerfRow>& Rows)
	{
		Rows.Sort([](const FNightfallPerfRow& A, const FNightfallPerfRow& B)
		{
			return A.Milliseconds > B.Milliseconds;
		});
		if (Rows.Num() > MaxRowsPerList)
		{
			Rows.SetNum(MaxRowsPerList, EAllowShrinking::No);
		}
	};

	SortDescending(SystemRows);
	SortDescending(GpuRows);
#endif // STATS
}

float UNightfallPerfSubsystem::GetBudgetHitRate() const
{
	return FramesSampled > 0 ? static_cast<float>(FramesWithinBudget) / static_cast<float>(FramesSampled) : 1.0f;
}

void UNightfallPerfSubsystem::ResetCounters()
{
	WorstFrameMs = 0.0f;
	WorstStreamingFrameMs = 0.0f;
	StreamingBusySeconds = 0.0f;
	HitchCount = 0;
	StreamingHitchCount = 0;
	FramesSampled = 0;
	FramesWithinBudget = 0;
}

void UNightfallPerfSubsystem::LogBreakdown() const
{
	LogSummary();

	auto Dump = [](const TCHAR* Heading, const TArray<FNightfallPerfRow>& Rows)
	{
		if (Rows.Num() == 0)
		{
			UE_LOG(LogNightfall, Log, TEXT("PERF %s: nothing recorded"), Heading);
			return;
		}
		UE_LOG(LogNightfall, Log, TEXT("PERF %s:"), Heading);
		for (const FNightfallPerfRow& Row : Rows)
		{
			UE_LOG(LogNightfall, Log, TEXT("PERF   %-40s %6.2f ms"), *Row.Name.ToString(), Row.Milliseconds);
		}
	};

	Dump(TEXT("systems"), SystemRows);
	Dump(TEXT("gpu passes"), GpuRows);

#if STATS
	// When the lists come back empty the question is always the same: did the stats system
	// hand us any groups at all, and were they the ones we asked for.
	if (SystemRows.Num() == 0 || GpuRows.Num() == 0)
	{
		const FGameThreadStatsData* Data = FLatestGameThreadStatsData::Get().Latest;
		if (!Data)
		{
			UE_LOG(LogNightfall, Warning, TEXT("PERF stats: no game thread data at all."));
		}
		else
		{
			FString Groups;
			for (const FName& Name : Data->GroupNames)
			{
				Groups += Name.ToString() + TEXT(" ");
			}
			UE_LOG(LogNightfall, Warning, TEXT("PERF stats: %d active groups: %s"),
				Data->GroupNames.Num(), *Groups);
		}
	}
#else
	UE_LOG(LogNightfall, Warning, TEXT("PERF stats are compiled out of this build."));
#endif
}

void UNightfallPerfSubsystem::CaptureAfter(float Seconds)
{
	CaptureCountdown = FMath::Max(Seconds, 0.0f);
	UE_LOG(LogNightfall, Log, TEXT("Screenshot scheduled in %.1f seconds."), CaptureCountdown);
}

void UNightfallPerfSubsystem::LogWorldReport() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Count by class name so nothing here has to know the project's own types, let alone
	// a plugin's.
	TMap<FName, int32> CountsByClass;
	int32 TotalActors = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		++TotalActors;
		const FName ClassName = Actor->GetClass()->GetFName();
		if (ClassName.ToString().StartsWith(TEXT("Nightfall")))
		{
			++CountsByClass.FindOrAdd(ClassName);
		}
	}

	UE_LOG(LogNightfall, Log, TEXT("WORLD %d actors total; project actors:"), TotalActors);

	CountsByClass.KeySort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	for (const TPair<FName, int32>& Entry : CountsByClass)
	{
		UE_LOG(LogNightfall, Log, TEXT("WORLD   %-32s %d"), *Entry.Key.ToString(), Entry.Value);
	}

	if (const UNightfallInteractionSubsystem* Interaction = World->GetSubsystem<UNightfallInteractionSubsystem>())
	{
		UE_LOG(LogNightfall, Log, TEXT("WORLD   interactables registered      %d"),
			Interaction->GetRegisteredCount());
	}

	// Instanced geometry is counted separately because it is not actors. Procedural scatter
	// that quietly generates nothing looks exactly like procedural scatter that works, and
	// once shipped that way: the graph produced no points and nobody could tell from a
	// screenshot of an already-busy field.
	int32 InstancedComponents = 0;
	int32 InstancedTotal = 0;
	TMap<FName, int32> InstancesByOwner;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		const UInstancedStaticMeshComponent* Component = *It;
		if (!Component || Component->GetWorld() != World || Component->IsTemplate())
		{
			continue;
		}

		const int32 Count = Component->GetInstanceCount();
		++InstancedComponents;
		InstancedTotal += Count;

		const AActor* Owner = Component->GetOwner();
		InstancesByOwner.FindOrAdd(Owner ? Owner->GetClass()->GetFName() : FName("(no owner)")) += Count;
	}

	UE_LOG(LogNightfall, Log, TEXT("WORLD   mesh instances               %d in %d component(s)"),
		InstancedTotal, InstancedComponents);
	InstancesByOwner.KeySort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	for (const TPair<FName, int32>& Entry : InstancesByOwner)
	{
		UE_LOG(LogNightfall, Log, TEXT("WORLD     %-30s %d"), *Entry.Key.ToString(), Entry.Value);
	}

	// The player's component list is the proof that the feature plugins attached: those
	// components are not in the pawn's constructor anywhere.
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		const FVector Location = Pawn->GetActorLocation();
		const FRotator View = Pawn->GetController()
			? Pawn->GetController()->GetControlRotation()
			: Pawn->GetActorRotation();
		UE_LOG(LogNightfall, Log,
			TEXT("WORLD player at (%.0f, %.0f, %.0f) looking (pitch %.1f, yaw %.1f), velocity %.0f"),
			Location.X, Location.Y, Location.Z, View.Pitch, View.Yaw, Pawn->GetVelocity().Size());
		UE_LOG(LogNightfall, Log, TEXT("WORLD player pawn '%s' components:"), *Pawn->GetClass()->GetName());
		for (const UActorComponent* Component : Pawn->GetComponents())
		{
			if (Component)
			{
				UE_LOG(LogNightfall, Log, TEXT("WORLD   %s"), *Component->GetClass()->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogNightfall, Warning, TEXT("WORLD no player pawn."));
	}
}
