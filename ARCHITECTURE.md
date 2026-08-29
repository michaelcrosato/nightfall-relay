# Architecture

Nightfall Relay is a first-person slice built so that adding gameplay never means editing
the game. The core is a C++ runtime module that knows about a world, a player, some
machines and a HUD. Everything that constitutes *play* arrives from Game Feature Plugins.

---

## The shape of it

```
Nightfall.uproject
├── Source/
│   ├── Nightfall/            runtime: world services, player, entities, UI
│   └── NightfallEditor/      editor-only: content authoring helpers
├── Plugins/GameFeatures/
│   ├── GridRestoration/      the gameplay loop
│   └── SurveyScanner/        a scan ability with its own input
├── Tools/                    the content build (Python, runs in a headless editor)
└── Content/Nightfall/        generated: nothing here is authored by hand
```

The dependency arrow points one way only. `GridRestoration` and `SurveyScanner` depend on
`Nightfall`. `Nightfall` contains no reference to either — no include, no cast, no class
name, no `if`. You can delete both plugin folders and the project still builds and runs;
you get a world with no objective and no scanner, which is exactly what should happen.

---

## Core runtime

### Services are subsystems

| Subsystem | Scope | Owns |
|---|---|---|
| `UNightfallWorldClockSubsystem` | World | The date, the time of day, the solar vector and the surface temperature |
| `UNightfallInteractionSubsystem` | World | Registry of every live interactable |
| `UNightfallPerfSubsystem` | World | Frame, GPU pass and streaming telemetry |
| `UNightfallUISubsystem` | Local player | The HUD, the settings menu, UI input |
| `UNightfallSaveSubsystem` | Game instance | Save and load |

The clock owns no lights. `ANightfallSkyDirector` reads the hour and drives the sun, moon,
sky light, fog and grade from a `UNightfallSkyProfile` data asset. Keeping those apart is
what lets a feature scrub time without touching lighting, and lets lighting be retuned
without touching gameplay.

Temperature is the clock's too, and for the same reason: it is a function of where the sun
has been, not of anything rendered. It is an exponentially weighted average of the solar
load over the preceding hours rather than a reading off the current altitude, which is why
the warmest hour of the cycle is in the afternoon and the coldest is the one before dawn.
Sampling backwards rather than filtering forwards keeps it a pure function of the hour, so
scrubbing or pausing the clock cannot leave the temperature disagreeing with the sky.

### Entities are rigid hierarchies

There is no skeletal animation in this project, and not by omission — `ANightfallCharacter`
passes `DoNotCreateDefaultSubobject(ACharacter::MeshComponentName)` so the pawn does not
even have the component `ACharacter` would normally give it.

Everything that moves is `ANightfallMachine`: a tree of static mesh components whose
relative transforms are written each frame.

| Entity | Motion |
|---|---|
| `ANightfallSentinelDrone` | Hull banks into its own velocity; yaw ring / pitch arm / sensor pod track or sweep; four rotors counter-spin |
| `ANightfallRelayPylon` | Mast telescopes with charge; two collector rings counter-rotate; collar creeps; emissive and two shadowed lights ramp together |
| `ANightfallBlastDoor` | Two leaves on an eased bounded travel; lock wheel turns through it |
| `ANightfallFilamentField` | Not a machine: instanced blades bent entirely in the vertex shader |
| `ANightfallPhysicsProp` | A simulating Chaos body, carried on a physics handle |

The player has two movement modes. On foot it is a `UCharacterMovementComponent` tuned for
mass. In **free flight** (`V`) the capsule's collision is dropped, movement follows the full
view direction rather than the ground plane, and jump and crouch become ascend and descend.
Flight is core rather than a plugin because it is the pawn's own traversal, not a mechanic.

`ANightfallMachine` gives all of them three things: appearance from a
`UNightfallMachineProfile`, a state gameplay tag other systems can observe, and one
`SetEmissiveLevel` call that drives every glowing surface at once.

### The four motion sources

| Source | Where |
|---|---|
| Rigid transform hierarchies | Every machine |
| Material world position offset | `M_NF_Filament`, driven by `ANightfallFilamentField` |
| Chaos rigid bodies | Power cells and rubble, carried and thrown |
| Niagara | Not used — see DECISIONS.md |

---

## How a feature attaches

This is the whole mechanism, and it is worth being precise about because everything else
follows from it.

1. Actors that can be extended call
   `UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver` in
   `PreInitializeComponents`, and send `NAME_GameActorReady` in `BeginPlay`.
   `ANightfallCharacter`, `ANightfallMachine` and `ANightfallPhysicsProp` all do.

2. Each plugin ships a `GameFeatureData` asset at its content root, named after the plugin,
   holding one `UGameFeatureAction_AddComponents`. That action is a list of
   *actor class → component class* pairs.

3. `"BuiltInInitialFeatureState": "Active"` in the `.uplugin` makes the engine activate the
   feature at startup. No bootstrap code anywhere.

GridRestoration's action, in full:

| Actor class | Component added |
|---|---|
| `ANightfallRelayPylon` | `UGridNodeComponent` — makes a pylon accept cells by hand |
| `ANightfallPhysicsProp` | `UGridCellComponent` — makes a tagged prop a power cell |
| `ANightfallSentinelDrone` | `UGridAlarmComponent` — makes being seen drain the grid |
| `ANightfallCharacter` | `UGridHudComponent` — puts the objective panel on the HUD |

Four classes gain behaviour. None of them were edited.

### The four seams a feature can use

A plugin never calls into the core to register itself. It uses whichever of these it needs:

- **Components by class.** The `AddComponents` action above.
- **Gameplay tags as data.** The level tags a prop `Nightfall.Grid.PowerCell`; the tag is
  declared by the plugin, and the core neither declares nor reads it. The prop just carries
  a `FGameplayTagContainer` it does not interpret.
- **HUD layers.** `UNightfallUISubsystem::RegisterHudPanel(LayerTag, SlateWidget)`. Panels
  registered before the HUD exists are queued and attached when it appears, so a feature
  that activates during startup does not have to care about ordering.
- **The stat group.** A counter declared against `STATGROUP_Nightfall` appears in the
  performance HUD's per-system list automatically. Both plugins declare one.

---

## Rendering

The brief is "few triangles, all the budget on light", and the whole renderer setup follows
from that.

- **Lumen** GI and reflections on hardware ray tracing.
- **Virtual shadow maps** for everything, so there are no cascades to tune.
- **MegaLights** for the local lights. Every pylon carries a shadowed point light and a
  shadowed spot; every drone a shadowed sensor beam; every door a shadowed rect light; every
  live power cell a shadowed point light. MegaLights does not handle directional lights, so
  the sun and moon go through VSM — which is the right split.
- **No Nanite.** At roughly 4,600 triangles per terrain tile and a few hundred per machine,
  cluster overhead costs more than it saves.
- **No static lighting.** Every light moves with the day.
- **Flat albedo, no textures.** The only texture in the project is the colour grading LUT.
  Surface interest comes from lighting; the albedo range across the whole palette is 0.017
  to 0.085, which is what makes emissive panels read as the brightest thing in frame.

Everything the settings menu changes is a console variable, and every one of those writes
goes through `NightfallCVar::Set`. It writes at `ECVF_SetByGameOverride` so a player choice
outranks the project's own renderer settings rather than being silently dropped by them,
and it reads the variable's tier back afterwards so a write that could not land says so.
See DECISIONS.md for the two toggles that shipped inert before it existed.

### Upscaling

`NightfallUpscaling` drives DLSS, Ray Reconstruction, frame generation and Reflex entirely
through the NVIDIA plugins' console variables, and detects each by looking the variable up.
Nothing links against those plugins. If they are absent the settings menu greys the rows
with a reason and the renderer falls back to TSR; drop the official plugins into `Plugins/`
and the same code drives them unchanged. See DECISIONS.md for why they are not shipped here.

---

## World

- **World Partition** with **One File Per Actor** — 195 external actor packages, 15 runtime cells.
- **Data layers**: `DL_Terrain`, `DL_Structures`, `DL_Machines`, all runtime layers created
  Activated so they can be switched off deliberately rather than starting empty.
- **HLODs** built by `WorldPartitionHLODsBuilder` — 9 HLOD actors, one per populated cell.
- **PCG** scatters debris across the compound from a graph built node by node in
  `UNightfallContentTools::BuildDebrisScatterGraph` — a seeded grid over the volume's floor,
  jittered in position, yaw and scale, feeding a weighted mesh spawner. A grid rather than a
  surface sampler because there is no landscape here for a sampler to sample.
- The region is 512 m of terrain in 16 tiles. Tiles are generated in world space and then
  recentred, so neighbours share exact vertex positions along their seam and no stitching
  pass is needed. The same height function is importable by the level build, so every actor
  is placed on the ground by arithmetic rather than by tracing.

---

## Performance HUD

`UNightfallPerfSubsystem` draws from three sources kept deliberately separate:

- **Frame timings** from the engine's thread counters and the RHI's GPU frame time history,
  read through a private cursor so nothing here competes with `stat unit`.
- **Per-system and GPU pass rows** from the stats system, with the groups enabled using
  `-nodisplay` so the data is collected without the engine drawing its own overlay on top.
- **Streaming cost** measured directly: cells in flight, seconds spent streaming, and
  whether the frames that went long did so while cells were loading.

`Nightfall.PerfLog 1` streams a summary line per second to the log; `Nightfall.PerfReport`
writes one on demand; `Nightfall.WorldReport` prints a census of what is actually loaded —
actors by class, registered interactables, the pawn's component list, and mesh instances
grouped by the actor that owns them, which is the only place instanced geometry shows up
at all.

---

## Content is built, not authored

Every asset under `Content/Nightfall` is generated by `Tools/build_content.py` running in a
headless editor. There are no hand-placed actors and no imported meshes.

```
UnrealEditor-Cmd.exe Nightfall.uproject -run=pythonscript ^
    -script="Tools/build_content.py" -unattended -nosplash
```

Four stages, selectable with `NF_STAGES`:

| Stage | Produces |
|---|---|
| `materials` | Two master materials and twelve instances |
| `meshes` | 42 static meshes, including 16 terrain tiles |
| `data` | Input assets, machine profiles, tuning table, grading LUT, both GameFeatureData assets |
| `level` | The World Partition map, data layers, and every actor in it |

Three operations cannot be expressed from script — game feature component entries are a
`USTRUCT` with no `BlueprintType`, DataTable rows are editor-only, and PCG's mesh selector
is an instanced object. Those live in `UNightfallContentTools` in the editor module, which
is what that module exists for.
