# Adding a feature

A new mechanic is a new Game Feature Plugin. It is never an edit to `Source/Nightfall`.

This document walks through **Survey Scanner**, which is in the repository and can be read
end to end, and then gives the checklist for your own.

---

## The worked example: Survey Scanner

A pulse that names what is around the player in the dark. It adds an ability, a key
binding, a HUD panel and a world-space cue — and the core project contains no reference to
any of it.

### What it is made of

```
Plugins/GameFeatures/SurveyScanner/
├── SurveyScanner.uplugin
├── Content/
│   ├── SurveyScanner.uasset          the GameFeatureData
│   └── Input/                        its own IA, IMC and input config
└── Source/SurveyScannerRuntime/
    ├── SurveyScannerRuntime.Build.cs
    ├── Public/
    │   ├── SurveyScannerRuntime.h    module, log category, stat counter
    │   ├── SurveyScannerTags.h       Nightfall.Scanner.Input.Pulse
    │   └── ScannerComponent.h        the ability
    └── Private/
        ├── SScannerPanel.h/.cpp      its HUD panel
        └── ...
```

### Step 1 — the plugin descriptor

Two fields matter:

```json
"ExplicitlyLoaded": true,
"BuiltInInitialFeatureState": "Active"
```

`ExplicitlyLoaded` is what makes it a game feature rather than an ordinary plugin.
`BuiltInInitialFeatureState` is what activates it at startup — there is no bootstrap code
anywhere in this project, and adding a feature does not add any.

The plugin is also listed in `Nightfall.uproject` so it gets mounted.

### Step 2 — depend on the game, and only that way round

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine", "GameplayTags",
    "ModularGameplay", "GameFeatures", "EnhancedInput",
    "Slate", "SlateCore",
    "Nightfall",          // the one dependency on the game
});
```

### Step 3 — write the behaviour as a component

`UScannerComponent` is an ordinary `UActorComponent`. Nothing constructs it; nothing
registers it. It assumes it is on a pawn and gets on with it.

Note what it reaches for, and what it does not:

```cpp
// Reads the core's registry rather than searching the world.
UNightfallInteractionSubsystem* Interaction = UNightfallInteractionSubsystem::Get(this);
const TArray<UNightfallInteractableComponent*> Found =
    Interaction->QueryInteractables(FGameplayTag(), Origin, ScanRadius);
```

It never casts to a pylon, a door or a cell. It asks the interaction registry, which is a
core service with no knowledge of scanning.

### Step 4 — bring your own input

The feature ships its own `UInputAction`, `UInputMappingContext` and `UNightfallInputConfig`
inside its own content folder, and pushes the context itself:

```cpp
Input->AddMappingContext(ResolvedInputConfig->MappingContext, MappingPriority);
InputComponent->BindAction(PulseAction, ETriggerEvent::Started, this, &UScannerComponent::Pulse);
```

and removes it again in `EndPlay`. The core input config has no scanner slot reserved in
it, because reserving one would be the core knowing about the feature.

Two wrinkles worth copying.

A pawn's `InputComponent` does not exist yet when its components begin play. The component
binds in `BeginPlay` *and* subscribes to `APawn::ReceiveRestartedDelegate`, so whichever
happens second does the work.

And if you generate a mapping context from script, **create its modifiers with the context
as their outer**. Enhanced Input modifiers are instanced UObjects; a transient one saves as
null, and the mapping survives without it. That failure is silent and total: every key on
the action ends up producing the same raw axis value. `Tools/dump_input.py` prints what a
context actually contains, which is how it was found.

### Step 5 — put something on the HUD

```cpp
Panel = SNew(SScannerPanel).Scanner(this);
UI->RegisterHudPanel(NightfallTags::UI_Layer_Hud, Panel.ToSharedRef());
```

and `UnregisterHudPanel` in `EndPlay`. The panel's lifetime is the feature's lifetime. The
core HUD only ever sees a widget arriving in a named layer.

### Step 6 — show up in the performance HUD

```cpp
DECLARE_CYCLE_STAT_EXTERN(TEXT("Survey Scanner"), STAT_Nightfall_SurveyScanner,
                          STATGROUP_Nightfall, SURVEYSCANNERRUNTIME_API);
```

Declaring against the core's stat group is all it takes. `SCOPE_CYCLE_COUNTER` in
`TickComponent`, and the cost appears as its own row. `UNightfallPerfSubsystem` was not
edited and does not know the name.

### Step 7 — bind it with a GameFeatureData

The asset lives at the plugin's content root and shares the plugin's name. It holds one
`UGameFeatureAction_AddComponents`:

| Actor class | Component |
|---|---|
| `ANightfallCharacter` | `UScannerComponent` |

Built by the content build, in `Tools/nf_data.py`:

```python
scanner = _build_feature_data("SurveyScanner", [
    (CORE + "NightfallCharacter", SCANNER + "ScannerComponent"),
])
```

That is the entire coupling between this feature and the game.

---

## Grid Restoration, for contrast

The same mechanism carrying a whole gameplay loop — four component bindings from one action:

| Actor class | Component | What it adds |
|---|---|---|
| `ANightfallRelayPylon` | `UGridNodeComponent` | Charge arrives only in the player's hands |
| `ANightfallPhysicsProp` | `UGridCellComponent` | Tagged props become power cells |
| `ANightfallSentinelDrone` | `UGridAlarmComponent` | Being seen drains the grid |
| `ANightfallCharacter` | `UGridHudComponent` | The objective panel |

`UGridCellComponent` is worth reading for one detail. The action attaches it to *every*
physics prop, and the component deactivates itself unless the placement carries
`Nightfall.Grid.PowerCell`:

```cpp
bIsCell = Prop->PropTags.HasTag(GridRestorationTags::Grid_PowerCell);
if (!bIsCell) { return; }
```

That tag is declared by the plugin. `ANightfallPhysicsProp` carries a
`FGameplayTagContainer` it never interprets. So one prop class covers rubble and power
cells, and the difference is data the feature owns.

---

## Your turn: the checklist

1. `Plugins/GameFeatures/<Name>/<Name>.uplugin` with `ExplicitlyLoaded` and
   `BuiltInInitialFeatureState: "Active"`.
2. `Source/<Name>Runtime/` with a `.Build.cs` depending on `Nightfall`, `ModularGameplay`,
   `GameFeatures` and whatever else you need.
3. A module `.h/.cpp` — copy `SurveyScannerRuntime.h`, including the stat counter.
4. Your components. Assume they land on the class you asked for; log and bail if not.
5. Add the plugin to the `Plugins` array in `Nightfall.uproject`.
6. If the plugin ships content, add `+DirectoriesToAlwaysCook=(Path="/<Name>")` to
   `Config/DefaultGame.ini`. The cooker does not follow soft references from C++ defaults,
   so a feature's own assets are silently left out of a package otherwise — which is how
   the scanner shipped its first build with no key binding.
7. Add a `_build_feature_data(...)` call to `build_feature_data()` in `Tools/nf_data.py`.
8. Rebuild the feature data:
   ```
   set NF_STAGES=data
   UnrealEditor-Cmd.exe Nightfall.uproject -run=pythonscript ^
       -script="Tools/build_content.py" -unattended -nosplash
   ```

Nothing on that list is a file under `Source/Nightfall`.

### If you find yourself wanting to edit the core

That usually means you want a seam that does not exist yet. There are four:

- a component added by class,
- a gameplay tag carried as data on an actor that does not interpret it,
- a HUD layer,
- a stat counter in `STATGROUP_Nightfall`.

Adding a fifth seam *is* a core change, and a legitimate one — a new layer tag, a new
service, a new extension point. Adding a special case for your feature is not. The
difference is whether the core ends up naming your feature.

### Removing a feature

Delete the folder and remove its line from `Nightfall.uproject`. That is the whole
procedure, and it is the real test of whether the boundary held.
