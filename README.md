# Nightfall Relay

A first-person open-world slice in Unreal Engine 5.8. Low-poly geometry lit expensively:
few triangles, and the whole visual budget spent on global illumination, shadows,
reflections and atmosphere.

You are restoring a derelict solar-relay field as the light goes. Carry power cells to
relay pylons; every pylon you energise lights the ground around it and pushes back the
dark. Sentinel drones patrol the field, and being seen bleeds charge back out of the grid.

There is no skeletal animation anywhere in the project — not one skeletal mesh, animation
blueprint or humanoid. Everything that moves is a rigid hierarchy of static meshes, a
material world position offset, or a Chaos rigid body.

---

## Build it

Requires **UE 5.8** and a C++ toolchain UE 5.8 accepts (MSVC ≥ 14.44.35211; note that UE
bans 14.44.0–14.44.35210).

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ^
    NightfallEditor Win64 Development -Project="%CD%\Nightfall.uproject"
```

### Generate the content

Every asset under `Content/Nightfall` is generated. A fresh clone has none of it, and this
is how it comes back:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%CD%\Nightfall.uproject" -run=pythonscript ^
    -script="%CD%\Tools\build_content.py" -unattended -nosplash
```

Roughly two minutes, and it produces 42 meshes, 15 materials, a colour grading LUT, the
input assets, both GameFeatureData assets, and the World Partition level. With HLODs built
the level holds 212 actors across 15 runtime cells. Set `NF_STAGES` to a comma separated
subset of `materials,meshes,data,level` to rebuild part of it.

### Build HLODs

After regenerating the level, rebuild its hierarchical LODs:

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%CD%\Nightfall.uproject" /Game/Nightfall/Maps/L_RelayField ^
    -run=WorldPartitionBuilderCommandlet -Builder=WorldPartitionHLODsBuilder ^
    -AllowCommandletRendering -unattended -nosplash
```

Do not pipe that through anything that closes the pipe early — it terminates the commandlet
partway through writing the level.

### Package

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
    -project="%CD%\Nightfall.uproject" -noP4 -platform=Win64 -clientconfig=Development ^
    -target=Nightfall -build -cook -map=/Game/Nightfall/Maps/L_RelayField ^
    -stage -pak -package -archive -archivedirectory="%CD%\Packaged" -prereqs
```

Output lands in `Packaged\Windows\Nightfall.exe`.

---

## Play it

| Input | Action |
|---|---|
| `W A S D` | Move |
| Mouse | Look |
| `Space` | Jump |
| `Shift` | Sprint (forward only) |
| `Ctrl` | Crouch |
| `E` | Interact — pick up a cell, or insert one into a pylon |
| `Q` | Throw what you are carrying |
| `F` | Survey pulse (from the Survey Scanner feature) |
| `T` | Phone light |
| `V` | Toggle free flight |
| `F1` | Cycle the performance HUD: hidden → compact → full |
| `Esc` | Settings |

Gamepad is mapped throughout.

### Free flight

`V` lifts the character into free flight: it drops collision so the camera passes through
the world, moves along the full view direction rather than the ground plane, and puts
ascend and descend on `Space` and `Ctrl`. `Shift` boosts to 3.5x. `V` again hands you back
to gravity wherever you are. `Nightfall.Fly [0|1]` does the same from the console.

It is the fastest way to see the whole 512 m field, and the fastest way to get to a pylon
when you are looking at the lighting rather than playing the loop.

The loop: find a power cell, carry it to a relay pylon, press `E`. Two cells bring a pylon
online. Six pylons restores the field.

Light cuts both ways. The sentinels pick out a lit figure from half again as far and settle
on it twice as fast, and a live power cell in your hands gives you away as surely as the
phone does — so the walk back to a pylon is the exposed leg of the run. Breaking line of
sight still resets what they had on you.

### Opening

The game starts at 17:51, with the sun about a degree above the horizon — the sun crosses it
at exactly 18:00 and the night key lands at 18:30, so the opening minute is the last of the
light. A briefing card explains that much and the loop, and the day clock is held for as long
as the card is up: at 36 minutes to the cycle one in-game hour costs ninety real seconds, so
without the hold a player who read to the end would arrive in full darkness. `Esc` dismisses
it and starts the clock; `Nightfall.Briefing` brings it back. Set
`bShowBriefingOnStart=False` under `[/Script/Nightfall.NightfallRuntimeSettings]` to skip it.

### The readout

Top right, above whatever panels the feature plugins have put there, is the date, the
clock and the temperature outside.

```
DAY 1              04 NOV 2231
18:00                      9°C
                          DUSK
```

The date advances: the clock counts midnights rather than discarding them, so a session
long enough to run past one reads `DAY 2` and the next date. Both survive a save.

The temperature is not a reading off the sun's altitude. The ground answers sunlight
slowly, so it is an exponentially weighted average of the sun that fell over the preceding
hours, which puts the warmest part of the cycle at 16:00 and the coldest at 05:30 rather
than at noon and midnight. Over the opening it falls from 9°C at 17:51 through 6°C at
18:30 to 1°C by 20:00, and bottoms out near -6°C before dawn. The number is coloured by
its own value, from sodium amber in the warm to hard blue below freezing, so the cost of
being out there after dark reads without being read.

`Nightfall.SetTime` and `Nightfall.SetDay` move all three. Set `TemperatureUnit=Fahrenheit`
under `[/Script/Nightfall.NightfallRuntimeSettings]` to read it in Fahrenheit; the start
date, the two ends of the temperature range and the thermal lag are configured in the same
section.

### Console commands

| Command | Effect |
|---|---|
| `Nightfall.PerfLog 1` | Stream a performance summary line per second to the log |
| `Nightfall.PerfReport` | Write the summary plus the per-system and GPU pass breakdowns |
| `Nightfall.PerfHud` | Cycle the performance HUD without the keyboard |
| `Nightfall.Settings` | Open or close the settings menu |
| `Nightfall.Briefing` | Show or dismiss the briefing card |
| `Nightfall.Flashlight [0\|1]` | The phone light; no argument toggles |
| `Nightfall.WorldReport` | Census of loaded actors, interactables and player components |
| `Nightfall.CaptureAfter <s>` | Screenshot once streaming and exposure have settled |
| `Nightfall.SetTime <hours>` | Scrub the day cycle |
| `Nightfall.SetDay <index>` | Jump to a day, counting from 0; the hour is left alone |
| `Nightfall.PauseTime <0\|1>` | Stop or resume the clock |
| `Nightfall.Fly [0\|1]` | Enter or leave free flight; no argument toggles |
| `Nightfall.TestMove [s] [fwd] [right]` | Drive movement input and report the distance travelled |
| `Grid.Report` | State of every relay node |
| `Grid.Energise <amount>` | Cheat: charge every node |
| `Nightfall.Save` / `Nightfall.Load` | Save and restore |

---

## Measured

RTX 4070 Super, 2560×1440, Development build, TSR at 100% render scale (no DLSS plugin
present — see DECISIONS.md):

| | |
|---|---|
| Frame rate | 138–188 fps |
| Frame | 5.3–7.2 ms against a 16.7 ms budget |
| GPU | 4.0–6.5 ms |
| Frames within budget | 100% after warm-up |
| Streaming hitches | 0 |

Measured from the outer corner with the whole field in view, at three points in the day
cycle: mid afternoon, golden hour, and night with all six pylons online and every drone
beam lit. The three are within a millisecond of each other.

At night, with every shadow-casting light in the scene switched on, the GPU breakdown from
the HUD's own rows reads: deferred lighting 1.98 ms, post processing 1.65, TSR 1.45, Lumen
screen probe gather 1.19, **MegaLights 0.74**, shadow depths 0.67, Lumen reflections 0.60,
volumetric fog 0.49.

The 60 fps floor is met with roughly 2.4x headroom *before* frame generation, which is the
point: frame generation is headroom here, not how the floor is reached.

---

## Documentation

- **ARCHITECTURE.md** — how the core, the services and the feature plugins fit together.
- **ADDING_A_FEATURE.md** — a worked example, walking through a plugin that is in the repo.
- **DECISIONS.md** — every choice, install and deferral, one line each.
