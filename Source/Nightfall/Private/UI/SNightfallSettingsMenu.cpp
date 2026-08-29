// Copyright Nightfall Relay. All Rights Reserved.

#include "UI/SNightfallSettingsMenu.h"

#include "NightfallGameUserSettings.h"
#include "NightfallUpscaling.h"
#include "UI/NightfallUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Nightfall"

namespace
{
	constexpr float LabelColumnWidth = 250.0f;
	constexpr float ValueColumnWidth = 200.0f;

	/** Wide enough for label, both arrows and the longest value without clipping. */
	constexpr float PanelWidth = 660.0f;


	/** Wrap an index into [0, Count) so stepping past either end rolls around. */
	int32 WrapIndex(int32 Index, int32 Count)
	{
		if (Count <= 0)
		{
			return 0;
		}
		return ((Index % Count) + Count) % Count;
	}

	FText ScalabilityLevelText(int32 Level)
	{
		switch (Level)
		{
		case 0:  return LOCTEXT("QualityLow", "Low");
		case 1:  return LOCTEXT("QualityMedium", "Medium");
		case 2:  return LOCTEXT("QualityHigh", "High");
		case 3:  return LOCTEXT("QualityEpic", "Epic");
		case 4:  return LOCTEXT("QualityCinematic", "Cinematic");
		default: return LOCTEXT("QualityCustom", "Custom");
		}
	}

	FText OnOffText(bool bOn)
	{
		return bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off");
	}

	/** Output resolutions offered by the menu. */
	const TArray<FIntPoint>& GetResolutionChoices()
	{
		static const TArray<FIntPoint> Choices = {
			FIntPoint(1280, 720),
			FIntPoint(1600, 900),
			FIntPoint(1920, 1080),
			FIntPoint(2560, 1440),
			FIntPoint(3440, 1440),
			FIntPoint(3840, 2160)
		};
		return Choices;
	}

	/** Frame rate caps offered by the menu. Zero means uncapped. */
	const TArray<float>& GetFrameRateChoices()
	{
		static const TArray<float> Choices = { 0.0f, 60.0f, 90.0f, 120.0f, 144.0f, 240.0f };
		return Choices;
	}
}

UNightfallGameUserSettings* SNightfallSettingsMenu::GetSettings()
{
	return UNightfallGameUserSettings::GetNightfallSettings();
}

void SNightfallSettingsMenu::Construct(const FArguments& InArgs)
{
	OnCloseRequested = InArgs._OnCloseRequested;

	const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);

	BuildDisplaySection(Rows);
	BuildUpscalingSection(Rows);
	BuildLightingSection(Rows);
	BuildQualitySection(Rows);

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBorder)
		.BorderImage(FNightfallUIStyle::SolidBrush())
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.006f, 0.008f, 0.012f, 0.96f)))
		.Padding(FMargin(34.0f, 28.0f))
		[
			SNew(SBox).WidthOverride(PanelWidth).MaxDesiredHeight(820.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 18.0f)
				[
					SNew(STextBlock)
					.Font(FNightfallUIStyle::GetMonoFont(20))
					.ColorAndOpacity(FSlateColor(FNightfallUIStyle::Accent()))
					.Text(LOCTEXT("SettingsTitle", "SETTINGS"))
				]

				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						Rows
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 0.0f)
				[
					BuildButtonBar()
				]
			]
		]
	];
}

// --- Row builders ----------------------------------------------------------------------

void SNightfallSettingsMenu::AddHeading(const TSharedRef<SVerticalBox>& Container, const FText& Label)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 6.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetMonoFont(11))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::PanelBorder()))
		.Text(Label)
	];
}

void SNightfallSettingsMenu::AddNote(const TSharedRef<SVerticalBox>& Container, TFunction<FText()> GetText)
{
	Container->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		SNew(STextBlock)
		.Font(FNightfallUIStyle::GetTextFont(10))
		.ColorAndOpacity(FSlateColor(FNightfallUIStyle::TextSecondary()))
		.AutoWrapText(true)
		.Text_Lambda([GetText]() { return GetText(); })
	];
}

void SNightfallSettingsMenu::AddStepperRow(
	const TSharedRef<SVerticalBox>& Container,
	const FText& Label,
	TFunction<FText()> GetValue,
	TFunction<void(int32)> Step,
	TFunction<bool()> IsEnabled)
{
	auto RowEnabled = [IsEnabled]() { return IsEnabled ? IsEnabled() : true; };

	auto MakeArrow = [Step, RowEnabled](const FText& Glyph, int32 Delta)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(10.0f, 2.0f))
			.IsEnabled_Lambda(RowEnabled)
			.OnClicked_Lambda([Step, Delta]()
			{
				Step(Delta);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(12))
				.Text(Glyph)
			];
	};

	Container->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(LabelColumnWidth)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetTextFont(12))
				.ColorAndOpacity_Lambda([RowEnabled]()
				{
					return FSlateColor(RowEnabled() ? FNightfallUIStyle::TextPrimary() : FNightfallUIStyle::TextSecondary());
				})
				.Text(Label)
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeArrow(FText::FromString(TEXT("<")), -1)
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(ValueColumnWidth).HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetMonoFont(12))
				.ColorAndOpacity_Lambda([RowEnabled]()
				{
					return FSlateColor(RowEnabled() ? FNightfallUIStyle::Accent() : FNightfallUIStyle::TextSecondary());
				})
				.Text_Lambda([GetValue]() { return GetValue(); })
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeArrow(FText::FromString(TEXT(">")), 1)
		]
	];
}

void SNightfallSettingsMenu::AddIndexRow(
	const TSharedRef<SVerticalBox>& Container,
	const FText& Label,
	TFunction<int32()> GetIndex,
	TFunction<void(int32)> SetIndex,
	TArray<FText> Options,
	TFunction<bool()> IsEnabled)
{
	const int32 Count = Options.Num();

	AddStepperRow(
		Container,
		Label,
		[GetIndex, Options, Count]()
		{
			const int32 Index = WrapIndex(GetIndex(), Count);
			return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
		},
		[GetIndex, SetIndex, Count](int32 Delta)
		{
			SetIndex(WrapIndex(GetIndex() + Delta, Count));
		},
		IsEnabled);
}

// --- Sections ----------------------------------------------------------------------------

void SNightfallSettingsMenu::BuildDisplaySection(const TSharedRef<SVerticalBox>& Container)
{
	AddHeading(Container, LOCTEXT("DisplayHeading", "-- display ----------------------------"));

	AddIndexRow(Container, LOCTEXT("WindowMode", "Window Mode"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings ? static_cast<int32>(Settings->GetFullscreenMode()) : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetFullscreenMode(static_cast<EWindowMode::Type>(Index));
			}
		},
		{ LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("WindowedFullscreen", "Windowed Fullscreen"), LOCTEXT("Windowed", "Windowed") },
		nullptr);

	AddStepperRow(Container, LOCTEXT("Resolution", "Resolution"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return FText::GetEmpty();
			}
			const FIntPoint Resolution = Settings->GetScreenResolution();
			return FText::FromString(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y));
		},
		[](int32 Delta)
		{
			UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return;
			}
			const TArray<FIntPoint>& Choices = GetResolutionChoices();
			const int32 Current = Choices.IndexOfByKey(Settings->GetScreenResolution());
			// An unlisted current resolution steps to the nearest listed one rather than
			// refusing to move.
			const int32 Start = (Current == INDEX_NONE) ? 0 : Current;
			Settings->SetScreenResolution(Choices[WrapIndex(Start + Delta, Choices.Num())]);
		},
		nullptr);

	AddIndexRow(Container, LOCTEXT("InvertLook", "Invert Look"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return (Settings && Settings->GetInvertLookY()) ? 1 : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetInvertLookY(Index != 0);
			}
		},
		{ OnOffText(false), OnOffText(true) },
		nullptr);

	AddIndexRow(Container, LOCTEXT("VSync", "V-Sync"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return (Settings && Settings->IsVSyncEnabled()) ? 1 : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetVSyncEnabled(Index != 0);
			}
		},
		{ OnOffText(false), OnOffText(true) },
		nullptr);

	AddStepperRow(Container, LOCTEXT("FrameRateLimit", "Frame Rate Limit"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			const float Limit = Settings ? Settings->GetFrameRateLimit() : 0.0f;
			return (Limit <= 0.0f)
				? LOCTEXT("Uncapped", "Uncapped")
				: FText::FromString(FString::Printf(TEXT("%.0f fps"), Limit));
		},
		[](int32 Delta)
		{
			UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return;
			}
			const TArray<float>& Choices = GetFrameRateChoices();
			int32 Current = Choices.IndexOfByPredicate([Settings](float Value)
			{
				return FMath::IsNearlyEqual(Value, Settings->GetFrameRateLimit());
			});
			if (Current == INDEX_NONE)
			{
				Current = 0;
			}
			Settings->SetFrameRateLimit(Choices[WrapIndex(Current + Delta, Choices.Num())]);
		},
		nullptr);
}

void SNightfallSettingsMenu::BuildUpscalingSection(const TSharedRef<SVerticalBox>& Container)
{
	AddHeading(Container, LOCTEXT("UpscalingHeading", "-- upscaling --------------------------"));
	AddNote(Container, []() { return NightfallUpscaling::GetAvailabilitySummary(); });

	auto DlssAvailable = []() { return NightfallUpscaling::IsDlssAvailable(); };
	auto UsingDlss = []()
	{
		const UNightfallGameUserSettings* Settings = GetSettings();
		return NightfallUpscaling::IsDlssAvailable() && Settings && Settings->GetUpscaler() == ENightfallUpscaler::DLSS;
	};

	AddIndexRow(Container, LOCTEXT("Upscaler", "Upscaler"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings ? static_cast<int32>(Settings->GetUpscaler()) : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetUpscaler(static_cast<ENightfallUpscaler>(Index));
			}
		},
		{ LOCTEXT("UpscalerNative", "Native"), LOCTEXT("UpscalerTSR", "TSR"),
		  LOCTEXT("UpscalerDLSS", "DLSS"), LOCTEXT("UpscalerDLAA", "DLAA") },
		nullptr);

	AddIndexRow(Container, LOCTEXT("DlssQuality", "DLSS Quality"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings ? static_cast<int32>(Settings->GetDlssQuality()) : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetDlssQuality(static_cast<ENightfallDlssQuality>(Index));
			}
		},
		{ LOCTEXT("DlssUltraPerf", "Ultra Performance"), LOCTEXT("DlssPerf", "Performance"),
		  LOCTEXT("DlssBalanced", "Balanced"), LOCTEXT("DlssQualityPreset", "Quality"),
		  LOCTEXT("DlssUltraQuality", "Ultra Quality") },
		UsingDlss);

	AddStepperRow(Container, LOCTEXT("RenderScale", "Render Scale"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings
				? FText::FromString(FString::Printf(TEXT("%.0f%%"), Settings->GetNativeScreenPercentage()))
				: FText::GetEmpty();
		},
		[](int32 Delta)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetNativeScreenPercentage(Settings->GetNativeScreenPercentage() + Delta * 5.0f);
			}
		},
		[]()
		{
			// Render scale only applies when DLSS is not choosing it for us.
			const UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return false;
			}
			const ENightfallUpscaler Mode = Settings->GetUpscaler();
			return Mode == ENightfallUpscaler::Native || Mode == ENightfallUpscaler::TSR
				|| !NightfallUpscaling::IsDlssAvailable();
		});

	AddIndexRow(Container, LOCTEXT("RayReconstruction", "Ray Reconstruction"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return (Settings && Settings->GetRayReconstruction()) ? 1 : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetRayReconstruction(Index != 0);
			}
		},
		{ OnOffText(false), OnOffText(true) },
		[]() { return NightfallUpscaling::IsRayReconstructionAvailable(); });

	AddIndexRow(Container, LOCTEXT("FrameGeneration", "Frame Generation (2x)"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return (Settings && Settings->GetFrameGeneration()) ? 1 : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetFrameGeneration(Index != 0);
			}
		},
		{ OnOffText(false), OnOffText(true) },
		[]() { return NightfallUpscaling::IsFrameGenerationAvailable(); });

	AddIndexRow(Container, LOCTEXT("Reflex", "Reflex Low Latency"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings ? static_cast<int32>(Settings->GetReflexMode()) : 0;
		},
		[](int32 Index)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetReflexMode(static_cast<ENightfallReflexMode>(Index));
			}
		},
		{ LOCTEXT("ReflexOff", "Off"), LOCTEXT("ReflexOn", "On"), LOCTEXT("ReflexBoost", "On + Boost") },
		[]() { return NightfallUpscaling::IsReflexAvailable(); });
}

void SNightfallSettingsMenu::BuildLightingSection(const TSharedRef<SVerticalBox>& Container)
{
	AddHeading(Container, LOCTEXT("LightingHeading", "-- lighting ---------------------------"));

	auto AddLightingToggle = [this, &Container](const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
	{
		AddIndexRow(Container, Label,
			[Get]() { return Get() ? 1 : 0; },
			[Set](int32 Index) { Set(Index != 0); },
			{ OnOffText(false), OnOffText(true) },
			nullptr);
	};

	AddLightingToggle(LOCTEXT("LumenHwrt", "Lumen Hardware Ray Tracing"),
		[]() { const UNightfallGameUserSettings* S = GetSettings(); return S && S->GetLumenHardwareRayTracing(); },
		[](bool bValue) { if (UNightfallGameUserSettings* S = GetSettings()) { S->SetLumenHardwareRayTracing(bValue); } });

	AddLightingToggle(LOCTEXT("MegaLights", "MegaLights"),
		[]() { const UNightfallGameUserSettings* S = GetSettings(); return S && S->GetMegaLights(); },
		[](bool bValue) { if (UNightfallGameUserSettings* S = GetSettings()) { S->SetMegaLights(bValue); } });

	AddLightingToggle(LOCTEXT("VirtualShadowMaps", "Virtual Shadow Maps"),
		[]() { const UNightfallGameUserSettings* S = GetSettings(); return S && S->GetVirtualShadowMaps(); },
		[](bool bValue) { if (UNightfallGameUserSettings* S = GetSettings()) { S->SetVirtualShadowMaps(bValue); } });

	AddLightingToggle(LOCTEXT("RtTranslucency", "Ray Traced Translucency"),
		[]() { const UNightfallGameUserSettings* S = GetSettings(); return S && S->GetRayTracedTranslucency(); },
		[](bool bValue) { if (UNightfallGameUserSettings* S = GetSettings()) { S->SetRayTracedTranslucency(bValue); } });

	AddStepperRow(Container, LOCTEXT("FogDistance", "Volumetric Fog Distance"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			return Settings
				? FText::FromString(FString::Printf(TEXT("%.0f m"), Settings->GetVolumetricFogDistance() / 100.0f))
				: FText::GetEmpty();
		},
		[](int32 Delta)
		{
			if (UNightfallGameUserSettings* Settings = GetSettings())
			{
				Settings->SetVolumetricFogDistance(Settings->GetVolumetricFogDistance() + Delta * 2000.0f);
			}
		},
		nullptr);
}

void SNightfallSettingsMenu::BuildQualitySection(const TSharedRef<SVerticalBox>& Container)
{
	AddHeading(Container, LOCTEXT("QualityHeading", "-- quality ----------------------------"));

	const TArray<FText> Levels = {
		ScalabilityLevelText(0), ScalabilityLevelText(1), ScalabilityLevelText(2),
		ScalabilityLevelText(3), ScalabilityLevelText(4)
	};

	// Overall is a stepper rather than an index row because the engine reports -1 when the
	// individual groups disagree, and calling that "Low" would be untrue.
	AddStepperRow(Container, LOCTEXT("OverallQuality", "Overall"),
		[]()
		{
			const UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return FText::GetEmpty();
			}
			const int32 Level = Settings->GetOverallScalabilityLevel();
			return (Level < 0) ? LOCTEXT("QualityMixed", "Custom") : ScalabilityLevelText(Level);
		},
		[](int32 Delta)
		{
			UNightfallGameUserSettings* Settings = GetSettings();
			if (!Settings)
			{
				return;
			}
			// A mixed set steps from Low, so the first press gives a defined result.
			const int32 Current = FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 4);
			Settings->SetOverallScalabilityLevel(WrapIndex(Current + Delta, 5));
		},
		nullptr);

	// Each group is the same shape, so declare them as data and loop.
	struct FQualityRow
	{
		FText Label;
		TFunction<int32(const UNightfallGameUserSettings*)> Get;
		TFunction<void(UNightfallGameUserSettings*, int32)> Set;
	};

	const TArray<FQualityRow> QualityRows = {
		{ LOCTEXT("ViewDistance", "View Distance"),
			[](const UNightfallGameUserSettings* S) { return S->GetViewDistanceQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetViewDistanceQuality(V); } },
		{ LOCTEXT("Shadows", "Shadows"),
			[](const UNightfallGameUserSettings* S) { return S->GetShadowQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetShadowQuality(V); } },
		{ LOCTEXT("GlobalIllumination", "Global Illumination"),
			[](const UNightfallGameUserSettings* S) { return S->GetGlobalIlluminationQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetGlobalIlluminationQuality(V); } },
		{ LOCTEXT("Reflections", "Reflections"),
			[](const UNightfallGameUserSettings* S) { return S->GetReflectionQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetReflectionQuality(V); } },
		{ LOCTEXT("PostProcessing", "Post Processing"),
			[](const UNightfallGameUserSettings* S) { return S->GetPostProcessingQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetPostProcessingQuality(V); } },
		{ LOCTEXT("Textures", "Textures"),
			[](const UNightfallGameUserSettings* S) { return S->GetTextureQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetTextureQuality(V); } },
		{ LOCTEXT("Effects", "Effects"),
			[](const UNightfallGameUserSettings* S) { return S->GetVisualEffectQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetVisualEffectQuality(V); } },
		{ LOCTEXT("Shading", "Shading"),
			[](const UNightfallGameUserSettings* S) { return S->GetShadingQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetShadingQuality(V); } },
		{ LOCTEXT("AntiAliasing", "Anti-Aliasing"),
			[](const UNightfallGameUserSettings* S) { return S->GetAntiAliasingQuality(); },
			[](UNightfallGameUserSettings* S, int32 V) { S->SetAntiAliasingQuality(V); } }
	};

	for (const FQualityRow& Row : QualityRows)
	{
		AddIndexRow(Container, Row.Label,
			[Get = Row.Get]()
			{
				const UNightfallGameUserSettings* Settings = GetSettings();
				return Settings ? FMath::Clamp(Get(Settings), 0, 4) : 0;
			},
			[Set = Row.Set](int32 Index)
			{
				if (UNightfallGameUserSettings* Settings = GetSettings())
				{
					Set(Settings, Index);
				}
			},
			Levels, nullptr);
	}
}

TSharedRef<SWidget> SNightfallSettingsMenu::BuildButtonBar()
{
	auto MakeButton = [](const FText& Label, TFunction<void()> OnPressed)
	{
		return SNew(SButton)
			.ContentPadding(FMargin(20.0f, 7.0f))
			.OnClicked_Lambda([OnPressed]()
			{
				OnPressed();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FNightfallUIStyle::GetTextFont(12))
				.Text(Label)
			];
	};

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			MakeButton(LOCTEXT("Apply", "Apply"), [this]() { ApplyAndSave(); })
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			MakeButton(LOCTEXT("Defaults", "Restore Defaults"), [this]() { RestoreDefaults(); })
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeButton(LOCTEXT("Close", "Close"), [this]()
			{
				OnCloseRequested.ExecuteIfBound();
			})
		];
}

void SNightfallSettingsMenu::ApplyAndSave()
{
	if (UNightfallGameUserSettings* Settings = GetSettings())
	{
		Settings->ApplySettings(/*bCheckForCommandLineOverrides=*/false);
		Settings->SaveSettings();
	}
}

void SNightfallSettingsMenu::RestoreDefaults()
{
	if (UNightfallGameUserSettings* Settings = GetSettings())
	{
		Settings->SetToDefaults();
		Settings->ApplySettings(/*bCheckForCommandLineOverrides=*/false);
		Settings->SaveSettings();
	}
}

#undef LOCTEXT_NAMESPACE
