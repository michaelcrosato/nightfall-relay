// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallPerfHud.h"

#include "Engine/World.h"
#include "NightfallPerfSubsystem.h"
#include "NightfallWorldClockSubsystem.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace
{
	/** Must match UNightfallPerfSubsystem's row cap. */
	constexpr int32 MaxRowSlots = 12;

	/** Width in characters of the name column, so values line up in the mono font. */
	constexpr int32 NameColumnWidth = 26;


	/** Left-justify and clip a name so the value column never moves. */
	FString PadName(const FString& Name)
	{
		FString Padded = Name.Left(NameColumnWidth);
		while (Padded.Len() < NameColumnWidth)
		{
			Padded.AppendChar(TEXT(' '));
		}
		return Padded;
	}
}

void SNightfallPerfHud::Construct(const FArguments& InArgs)
{
	WeakWorld = InArgs._World;
	ModeAttribute = InArgs._Mode;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	// --- Heading: title and the in-game clock -----------------------------------------
	Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(11))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
		.Text_Lambda([this]()
		{
			FString Clock = TEXT("--:--");
			if (const UWorld* World = WeakWorld.Get())
			{
				if (const UNightfallWorldClockSubsystem* WorldClock = World->GetSubsystem<UNightfallWorldClockSubsystem>())
				{
					Clock = WorldClock->GetClockString();
				}
			}
			return FText::FromString(FString::Printf(TEXT("NIGHTFALL RELAY        %s"), *Clock));
		})
	];

	// --- Frame rate and frame time, coloured against the budget ------------------------
	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(13))
		.ColorAndOpacity_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			return FSlateColor(Perf
				? FNightfallUIStyle::ColorForBudget(Perf->GetFrameMilliseconds(), Perf->GetFrameBudgetMilliseconds())
				: FNightfallUIStyle::TextSecondary());
		})
		.Text_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return LOCTEXT("PerfUnavailable", "perf subsystem unavailable");
			}
			return FText::FromString(FString::Printf(
				TEXT("%6.1f fps   %5.2f ms   budget %4.1f"),
				Perf->GetFramesPerSecond(),
				Perf->GetFrameMilliseconds(),
				Perf->GetFrameBudgetMilliseconds()));
		})
	];

	// --- Thread and GPU breakdown ------------------------------------------------------
	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextPrimary()))
		.Visibility_Lambda([this]() { return GetVisibilityForMode(ENightfallPerfHudMode::Full); })
		.Text_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return FText::GetEmpty();
			}
			return FText::FromString(FString::Printf(
				TEXT("game %5.2f  render %5.2f  rhi %5.2f  gpu %5.2f"),
				Perf->GetGameThreadMilliseconds(),
				Perf->GetRenderThreadMilliseconds(),
				Perf->GetRhiThreadMilliseconds(),
				Perf->GetGpuMilliseconds()));
		})
	];

	// --- Budget verdict over the session ------------------------------------------------
	Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
		.Text_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return FText::GetEmpty();
			}
			return FText::FromString(FString::Printf(
				TEXT("in budget %5.1f%%   worst %5.2f ms"),
				Perf->GetBudgetHitRate() * 100.0f,
				Perf->GetWorstFrameMilliseconds()));
		})
	];

	// --- Per-system rows -----------------------------------------------------------------
	Body->AddSlot().AutoHeight()[ MakeSectionHeading(LOCTEXT("SystemsHeading", "-- systems ------------------")) ];
	for (int32 Index = 0; Index < MaxRowSlots; ++Index)
	{
		Body->AddSlot().AutoHeight()[ MakeRowSlot(Index, /*bGpuList=*/false) ];
	}

	// --- GPU pass rows -------------------------------------------------------------------
	Body->AddSlot().AutoHeight()[ MakeSectionHeading(LOCTEXT("GpuHeading", "-- gpu passes ---------------")) ];
	for (int32 Index = 0; Index < MaxRowSlots; ++Index)
	{
		Body->AddSlot().AutoHeight()[ MakeRowSlot(Index, /*bGpuList=*/true) ];
	}

	// --- Streaming cost --------------------------------------------------------------------
	Body->AddSlot().AutoHeight()[ MakeSectionHeading(LOCTEXT("StreamingHeading", "-- streaming ----------------")) ];
	Body->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.Visibility_Lambda([this]() { return GetVisibilityForMode(ENightfallPerfHudMode::Full); })
		.ColorAndOpacity_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			// Any hitch attributed to streaming is a budget failure worth shouting about,
			// because the brief says streaming must not hitch.
			return FSlateColor((Perf && Perf->GetStreamingHitchCount() > 0)
				? FNightfallUIStyle::Bad()
				: FNightfallUIStyle::TextPrimary());
		})
		.Text_Lambda([this]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return FText::GetEmpty();
			}
			return FText::FromString(FString::Printf(
				TEXT("cells %2d  busy %5.1fs\nhitches %d (%d streaming)\nworst while streaming %5.2f ms"),
				Perf->GetPendingStreamingCells(),
				Perf->GetStreamingBusySeconds(),
				Perf->GetHitchCount(),
				Perf->GetStreamingHitchCount(),
				Perf->GetWorstStreamingFrameMilliseconds()));
		})
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FNightfallUIStyle::SolidBrush())
		.BorderBackgroundColor(FSlateColor(FNightfallUIStyle::PanelBackground()))
		.Padding(FMargin(10.0f, 8.0f))
		.Visibility_Lambda([this]() { return GetVisibilityForMode(ENightfallPerfHudMode::Compact); })
		[
			SNew(SBox).MinDesiredWidth(320.0f)
			[
				Body
			]
		]
	];
}

TSharedRef<SWidget> SNightfallPerfHud::MakeSectionHeading(const FText& Label)
{
	return SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(9))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::PanelBorder()))
		.Visibility_Lambda([this]() { return GetVisibilityForMode(ENightfallPerfHudMode::Full); })
		.Text(Label);
}

TSharedRef<SWidget> SNightfallPerfHud::MakeRowSlot(int32 Index, bool bGpuList)
{
	return SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(10))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextPrimary()))
		.Visibility_Lambda([this, Index, bGpuList]()
		{
			if (GetVisibilityForMode(ENightfallPerfHudMode::Full) != EVisibility::Visible)
			{
				return EVisibility::Collapsed;
			}

			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return EVisibility::Collapsed;
			}

			const TArray<FNightfallPerfRow>& Rows = bGpuList ? Perf->GetGpuRows() : Perf->GetSystemRows();
			return Rows.IsValidIndex(Index) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.Text_Lambda([this, Index, bGpuList]()
		{
			const UNightfallPerfSubsystem* Perf = GetPerf();
			if (!Perf)
			{
				return FText::GetEmpty();
			}

			const TArray<FNightfallPerfRow>& Rows = bGpuList ? Perf->GetGpuRows() : Perf->GetSystemRows();
			if (!Rows.IsValidIndex(Index))
			{
				return FText::GetEmpty();
			}

			const FNightfallPerfRow& Row = Rows[Index];
			return FText::FromString(FString::Printf(
				TEXT("%s%6.2f"), *PadName(Row.Name.ToString()), Row.Milliseconds));
		});
}

EVisibility SNightfallPerfHud::GetVisibilityForMode(ENightfallPerfHudMode Minimum) const
{
	const ENightfallPerfHudMode Current = ModeAttribute.Get(ENightfallPerfHudMode::Hidden);
	return static_cast<uint8>(Current) >= static_cast<uint8>(Minimum) ? EVisibility::Visible : EVisibility::Collapsed;
}

UNightfallPerfSubsystem* SNightfallPerfHud::GetPerf() const
{
	const UWorld* World = WeakWorld.Get();
	return World ? World->GetSubsystem<UNightfallPerfSubsystem>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
