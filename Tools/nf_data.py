"""Data assets: input, appearance profiles, tuning tables, the grade, and the two
GameFeatureData assets that make the feature plugins do anything.

The GameFeatureData assets are the interesting part. Each one is a list of
GameFeatureAction_AddComponents entries saying "when this feature is active, put this
component on every actor of this class". That is the entire coupling between a feature and
the game: no registration call, no core edit, no cast.
"""

import os
import struct
import zlib

import unreal

from nf_common import PATHS, banner, create, create_at, linear, load, rot, save, vec

_asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


# --- input --------------------------------------------------------------------------------

def _make_action(folder, name, value_type):
    action = create(folder, name, unreal.InputAction, None)
    action.set_editor_property("value_type", value_type)
    save(action)
    return action


# Enhanced Input modifiers are instanced UObjects that live inside the mapping context's
# package. One constructed with a bare unreal.InputModifierNegate() is transient and
# serialises as null - the mapping survives, the modifier does not, and every key ends up
# producing the same raw axis value. They must be created with the context as their outer.
def _negate(outer, x=False, y=False, z=False):
    modifier = unreal.new_object(unreal.InputModifierNegate, outer=outer)
    modifier.set_editor_property("bx", x)
    modifier.set_editor_property("by", y)
    modifier.set_editor_property("bz", z)
    return modifier


def _swizzle_yxz(outer):
    modifier = unreal.new_object(unreal.InputModifierSwizzleAxis, outer=outer)
    modifier.set_editor_property("order", unreal.InputAxisSwizzle.YXZ)
    return modifier


def _key(key_name):
    """FKey has no positional constructor; it is a struct with a name field."""
    key = unreal.Key()
    key.set_editor_property("key_name", key_name)
    return key


def _mapping(action, key_name, modifiers=None):
    mapping = unreal.EnhancedActionKeyMapping()
    mapping.set_editor_property("action", action)
    mapping.set_editor_property("key", _key(key_name))
    if modifiers:
        mapping.set_editor_property("modifiers", modifiers)
    return mapping


# UInputMappingContext.Mappings is deprecated since UE 5.7 and the runtime rebuild reads
# DefaultKeyMappings.Mappings instead. PostLoad migrates the old array only for assets
# saved before the format change - an asset authored by this editor saves with the current
# version, so mappings written to the deprecated slot register fine and then apply as zero
# key bindings: every physical key dead, with nothing logged anywhere.
def _apply_mappings(context, mappings):
    data = unreal.InputMappingContextMappingData()
    data.set_editor_property("mappings", mappings)
    context.set_editor_property("default_key_mappings", data)
    # Scrub the deprecated slot so a reused asset does not keep stale mappings there.
    context.set_editor_property("mappings", [])


def build_input():
    """The core input config, plus the scanner feature's own."""
    banner("input")

    boolean = unreal.InputActionValueType.BOOLEAN
    axis2d = unreal.InputActionValueType.AXIS2D

    actions = {
        "Move": _make_action("input", "IA_NF_Move", axis2d),
        "Look": _make_action("input", "IA_NF_Look", axis2d),
        "Jump": _make_action("input", "IA_NF_Jump", boolean),
        "Sprint": _make_action("input", "IA_NF_Sprint", boolean),
        "Crouch": _make_action("input", "IA_NF_Crouch", boolean),
        "Interact": _make_action("input", "IA_NF_Interact", boolean),
        "Drop": _make_action("input", "IA_NF_Drop", boolean),
        "ToggleFly": _make_action("input", "IA_NF_ToggleFly", boolean),
        "ToggleMenu": _make_action("input", "IA_NF_ToggleMenu", boolean),
        "TogglePerfHud": _make_action("input", "IA_NF_TogglePerfHud", boolean),
        "ToggleFlashlight": _make_action("input", "IA_NF_ToggleFlashlight", boolean),
    }

    context = create("input", "IMC_NF_Default", unreal.InputMappingContext, None)
    _apply_mappings(context, [
        # WASD into a 2D axis. A key produces 1 on X, so forward and back are swizzled onto
        # Y - which is what ANightfallCharacter::Input_Move reads as forward.
        #
        # Modifier order matters: they apply left to right. S negates *after* the swizzle,
        # so it must negate Y; negating X there would act on the component the swizzle just
        # emptied and S would drive forward.
        _mapping(actions["Move"], "W", [_swizzle_yxz(context)]),
        _mapping(actions["Move"], "S", [_swizzle_yxz(context), _negate(context, y=True)]),
        _mapping(actions["Move"], "A", [_negate(context, x=True)]),
        _mapping(actions["Move"], "D"),
        _mapping(actions["Move"], "Gamepad_Left2D"),

        # No negate on mouse Y, and that is deliberate. The stock template negates it to
        # cancel APlayerController's InputPitchScale of -2.5, but that scale is only applied
        # when bEnableLegacyInputScales is true, and DefaultInput.ini turns it off so a mouse
        # delta reaches the camera at the size it was authored. With the scale gone there is
        # no negative left to cancel, and negating here is what inverted the pitch. The
        # gamepad stick never had one, which is why the two disagreed.
        _mapping(actions["Look"], "Mouse2D"),
        _mapping(actions["Look"], "Gamepad_Right2D"),

        _mapping(actions["Jump"], "SpaceBar"),
        _mapping(actions["Jump"], "Gamepad_FaceButton_Bottom"),
        _mapping(actions["Sprint"], "LeftShift"),
        _mapping(actions["Sprint"], "Gamepad_LeftThumbstick"),
        _mapping(actions["Crouch"], "LeftControl"),
        _mapping(actions["Crouch"], "Gamepad_RightThumbstick"),
        _mapping(actions["Interact"], "E"),
        _mapping(actions["Interact"], "Gamepad_FaceButton_Right"),
        _mapping(actions["Drop"], "Q"),
        _mapping(actions["Drop"], "Gamepad_FaceButton_Left"),
        _mapping(actions["ToggleFly"], "V"),
        _mapping(actions["ToggleFly"], "Gamepad_LeftShoulder"),
        _mapping(actions["ToggleMenu"], "Escape"),
        _mapping(actions["ToggleMenu"], "Gamepad_Special_Right"),
        _mapping(actions["TogglePerfHud"], "F1"),
        _mapping(actions["TogglePerfHud"], "Gamepad_Special_Left"),

        # T for torch. F is the obvious key and is taken by the scanner pulse, and Enhanced
        # Input fires every action mapped to a pressed key across applied contexts, so
        # sharing it would pulse the scanner every time the light went on.
        _mapping(actions["ToggleFlashlight"], "T"),
        _mapping(actions["ToggleFlashlight"], "Gamepad_RightShoulder"),
    ])
    save(context)

    config = create("input", "IC_NF_Default", unreal.NightfallInputConfig, None)
    config.set_editor_property("mapping_context", context)
    config.set_editor_property("mapping_priority", 0)
    config.set_editor_property("actions", [
        _binding("Nightfall.Input.Move", actions["Move"]),
        _binding("Nightfall.Input.Look", actions["Look"]),
        _binding("Nightfall.Input.Jump", actions["Jump"]),
        _binding("Nightfall.Input.Sprint", actions["Sprint"]),
        _binding("Nightfall.Input.Crouch", actions["Crouch"]),
        _binding("Nightfall.Input.Interact", actions["Interact"]),
        _binding("Nightfall.Input.Drop", actions["Drop"]),
        _binding("Nightfall.Input.ToggleFly", actions["ToggleFly"]),
        _binding("Nightfall.Input.ToggleMenu", actions["ToggleMenu"]),
        _binding("Nightfall.Input.TogglePerfHud", actions["TogglePerfHud"]),
        _binding("Nightfall.Input.ToggleFlashlight", actions["ToggleFlashlight"]),
    ])
    save(config)

    unreal.log("core input: %d actions" % len(actions))
    return config


_tag_library = getattr(unreal, "GameplayTagLibrary", None) or     getattr(unreal, "BlueprintGameplayTagLibrary", None)


def _tag(tag_name):
    """Resolve a registered gameplay tag by name.

    The blueprint tag library only offers comparison helpers, not a lookup, so go through
    the struct's own text import - which is exactly what FGameplayTag::ImportTextItem does
    when a tag is typed into a property. It resolves against the registered tag table, so a
    name that was never declared in C++ fails here rather than silently binding nothing.
    """
    tag = unreal.GameplayTag()
    tag.import_text(tag_name)

    if not _tag_library.is_gameplay_tag_valid(tag):
        raise RuntimeError(
            "gameplay tag '%s' is not registered; check the native tag declarations" % tag_name)
    return tag


def _binding(tag_name, action):
    binding = unreal.NightfallInputActionBinding()
    binding.set_editor_property("input_tag", _tag(tag_name))
    binding.set_editor_property("input_action", action)
    return binding


def build_scanner_input():
    """The survey scanner ships its own action, context and config inside its plugin."""
    folder = "/SurveyScanner/Input"

    def make(name, asset_class):
        return create_at(folder, name, asset_class)

    action = make("IA_Scanner_Pulse", unreal.InputAction)
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)

    context = make("IMC_Scanner", unreal.InputMappingContext)
    _apply_mappings(context, [
        _mapping(action, "F"),
        _mapping(action, "Gamepad_FaceButton_Top"),
    ])
    save(context)

    config = make("IC_Scanner", unreal.NightfallInputConfig)
    config.set_editor_property("mapping_context", context)
    config.set_editor_property("mapping_priority", 10)
    config.set_editor_property("actions", [
        _binding("Nightfall.Scanner.Input.Pulse", action),
    ])
    save(config)

    unreal.log("scanner input built inside its own plugin")
    return config


# --- colour grading LUT ------------------------------------------------------------------

def _write_png(path, width, height, rows):
    """Minimal 8 bit RGB PNG writer. Avoids needing an image library in the editor."""
    raw = b"".join(b"\x00" + row for row in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    blob = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", header)
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))

    with open(path, "wb") as handle:
        handle.write(blob)


def _grade(r, g, b):
    """The look, as a function.

    Desaturate a little, add contrast around a low pivot so the darks stay dark, then split
    tone: shadows toward blue, highlights toward sodium. This is the same operation the
    lighting is already doing with colour; baking it into a LUT locks it in after tonemapping
    so it survives the exposure swing between noon and midnight.
    """
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b

    saturation = 0.88
    r = luma + (r - luma) * saturation
    g = luma + (g - luma) * saturation
    b = luma + (b - luma) * saturation

    pivot, contrast = 0.44, 1.16
    r = (r - pivot) * contrast + pivot
    g = (g - pivot) * contrast + pivot
    b = (b - pivot) * contrast + pivot

    weight = max(0.0, min(1.0, luma))
    weight = weight * weight * (3.0 - 2.0 * weight)
    shadow_tint = (0.94, 0.99, 1.13)
    highlight_tint = (1.07, 1.005, 0.90)
    r *= shadow_tint[0] + (highlight_tint[0] - shadow_tint[0]) * weight
    g *= shadow_tint[1] + (highlight_tint[1] - shadow_tint[1]) * weight
    b *= shadow_tint[2] + (highlight_tint[2] - shadow_tint[2]) * weight

    # A small toe keeps the deepest shadows from crushing to pure black, which reads as
    # missing geometry rather than as darkness.
    lift = 0.011
    r = lift + r * (1.0 - lift)
    g = lift + g * (1.0 - lift)
    b = lift + b * (1.0 - lift)

    return (max(0.0, min(1.0, r)), max(0.0, min(1.0, g)), max(0.0, min(1.0, b)))


def build_lut():
    """Generate and import a 16 cube colour grading LUT as a 256x16 strip."""
    banner("colour grading LUT")

    size = 16
    rows = []
    for v in range(size):
        row = bytearray()
        for slice_index in range(size):
            for u in range(size):
                r, g, b = _grade(u / (size - 1.0), v / (size - 1.0), slice_index / (size - 1.0))
                row += bytes((int(r * 255.0 + 0.5), int(g * 255.0 + 0.5), int(b * 255.0 + 0.5)))
        rows.append(bytes(row))

    temp_dir = os.path.join(unreal.Paths.project_saved_dir(), "Temp")
    os.makedirs(temp_dir, exist_ok=True)
    png_path = os.path.abspath(os.path.join(temp_dir, "NF_Grade.png"))
    _write_png(png_path, size * size, size, rows)

    target = "%s/T_NF_Grade" % PATHS["textures"]
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        unreal.EditorAssetLibrary.delete_asset(target)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png_path)
    task.set_editor_property("destination_path", PATHS["textures"])
    task.set_editor_property("destination_name", "T_NF_Grade")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.TextureFactory())
    _asset_tools.import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(target)
    if texture is None:
        unreal.log_error("LUT import produced no asset")
        return None

    # A LUT must not be filtered across slices or mipped, or neighbouring colours bleed.
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_COLOR_LOOKUP_TABLE)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    save(texture)

    unreal.log("LUT built: %dx%d" % (size * size, size))
    return texture


# --- profiles ---------------------------------------------------------------------------------

def _part(name, mesh, material, location=None, rotation=None, scale=None, emissive=False):
    part = unreal.NightfallMachinePart()
    part.set_editor_property("part_name", name)
    part.set_editor_property("mesh", mesh)
    part.set_editor_property("material_override", material)
    part.set_editor_property("relative_location", location or vec())
    part.set_editor_property("relative_rotation", rotation or rot())
    part.set_editor_property("relative_scale", scale or vec(1.0, 1.0, 1.0))
    part.set_editor_property("emissive", emissive)
    return part


def build_sky_profile(lut):
    """The four lighting keys come from the class defaults; this asset adds the grade.

    The keys are re-copied from the class default object every build. Without that, an
    asset created by an earlier run keeps the values it was born with and retuning the
    lighting in C++ would silently do nothing.
    """
    profile = create("data", "DA_NF_SkyProfile", unreal.NightfallSkyProfile, None)

    defaults = unreal.NightfallSkyProfile.get_default_object()
    for key in ("night", "dawn", "day", "dusk"):
        profile.set_editor_property(key, defaults.get_editor_property(key))

    profile.set_editor_property("color_grading_lut", lut)
    profile.set_editor_property("color_grading_intensity", 0.85)
    save(profile)
    return profile


def build_machine_profiles(materials, meshes):
    banner("machine profiles")
    profiles = {}

    pylon_meshes = meshes["pylon"]
    pylon = create("data", "DA_NF_PylonProfile", unreal.NightfallMachineProfile, None)
    pylon.set_editor_property("parts", [
        _part("Base", pylon_meshes["Base"], materials["Concrete"]),
        _part("Collar", pylon_meshes["Collar"], materials["SteelDark"], vec(0, 0, 140)),
        # The mast sits at zero and the actor drives its Z; at rest it is sunk in the plinth.
        _part("Mast", pylon_meshes["Mast"], materials["Steel"], vec(0, 0, 0)),
        _part("RingLower", pylon_meshes["RingLower"], materials["SteelWorn"], vec(0, 0, 300)),
        _part("RingUpper", pylon_meshes["RingUpper"], materials["SteelWorn"], vec(0, 0, 432)),
        _part("Core", pylon_meshes["Core"], materials["PanelAmber"], vec(0, 0, 560), emissive=True),
    ])
    pylon.set_editor_property("accent_color", linear(1.0, 0.62, 0.18))
    pylon.set_editor_property("accent_intensity", 11.0)
    save(pylon)
    profiles["pylon"] = pylon

    drone_meshes = meshes["drone"]
    drone = create("data", "DA_NF_DroneProfile", unreal.NightfallMachineProfile, None)
    drone_parts = [
        _part("Hull", drone_meshes["Hull"], materials["SteelDark"]),
        _part("YawRing", drone_meshes["YawRing"], materials["Steel"], vec(0, 0, -34)),
        _part("PitchArm", drone_meshes["PitchArm"], materials["Steel"], vec(0, 0, -50)),
        _part("SensorPod", drone_meshes["SensorPod"], materials["PanelCyan"], vec(0, 0, 0), emissive=True),
    ]
    # Four rotors on the arm ends, on the diagonals.
    for index, (x, y) in enumerate([(134, 134), (-134, 134), (-134, -134), (134, -134)]):
        drone_parts.append(
            _part("Rotor%s" % "ABCD"[index], drone_meshes["Rotor"], materials["SteelWorn"],
                  vec(x, y, 26)))
    drone.set_editor_property("parts", drone_parts)
    drone.set_editor_property("accent_color", linear(0.24, 0.72, 1.0))
    drone.set_editor_property("accent_intensity", 9.0)
    save(drone)
    profiles["drone"] = drone

    door_meshes = meshes["door"]
    door = create("data", "DA_NF_DoorProfile", unreal.NightfallMachineProfile, None)
    door.set_editor_property("parts", [
        _part("Frame", door_meshes["Frame"], materials["Concrete"]),
        _part("LeafLeft", door_meshes["Leaf"], materials["SteelDark"], vec(0, -110, 240)),
        _part("LeafRight", door_meshes["Leaf"], materials["SteelDark"], vec(0, 110, 240)),
        # Pitched upright so the wheel spins about X, which is the axis the door drives.
        _part("LockWheel", door_meshes["Wheel"], materials["SteelWorn"], vec(60, 0, 240), rot(90, 0, 0)),
        _part("StatusPanel", door_meshes["Panel"], materials["PanelAmber"], vec(52, 0, 430), emissive=True),
    ])
    door.set_editor_property("accent_color", linear(1.0, 0.55, 0.06))
    door.set_editor_property("accent_intensity", 10.0)
    save(door)
    profiles["door"] = door

    unreal.log("machine profiles: %d" % len(profiles))
    return profiles


def build_tuning_table():
    """Sentinel behaviour presets."""
    banner("tuning table")

    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.NightfallSentinelTuningRow.static_struct())
    table = create("data", "DT_NF_SentinelTuning", unreal.DataTable, factory)
    if table is None:
        unreal.log_error("could not create the sentinel tuning table")
        return None

    rows = {
        # Wide slow sweeps. The bulk of the field.
        "Patrol": dict(patrol_speed=330.0, alert_speed=610.0, detection_range=2900.0,
                       detection_half_angle=42.0, time_to_acquire=0.75, time_to_lose=2.6,
                       investigate_duration=6.0, beam_intensity=11000.0),
        # Parked over a pylon, narrow cone, very quick to commit.
        "Watchtower": dict(patrol_speed=140.0, alert_speed=430.0, detection_range=3600.0,
                           detection_half_angle=26.0, time_to_acquire=0.42, time_to_lose=3.6,
                           investigate_duration=9.0, beam_intensity=15000.0),
        # Fast, short sighted, and hard to shake once it has you.
        "Hunter": dict(patrol_speed=470.0, alert_speed=820.0, detection_range=2200.0,
                       detection_half_angle=58.0, time_to_acquire=0.55, time_to_lose=4.4,
                       investigate_duration=11.0, beam_intensity=9000.0),
    }

    for row_name, values in rows.items():
        row = unreal.NightfallSentinelTuningRow()
        for key, value in values.items():
            row.set_editor_property(key, value)
        if not unreal.NightfallContentTools.add_sentinel_tuning_row(table, row_name, row):
            unreal.log_error("could not add tuning row '%s'" % row_name)
            return None

    save(table)
    unreal.log("sentinel tuning: %d rows" % len(rows))
    return table


# --- game feature data ---------------------------------------------------------------------

#: Class paths the feature actions bind. Written out rather than derived so a rename
#: shows up as a build failure with the old name in it.
CORE = "/Script/Nightfall."
GRID = "/Script/GridRestorationRuntime."
SCANNER = "/Script/SurveyScannerRuntime."


def _build_feature_data(plugin_name, bindings):
    """Create a plugin's GameFeatureData with a single AddComponents action.

    FGameFeatureComponentEntry is a plain USTRUCT with no BlueprintType, so it cannot be
    built from script; UNightfallContentTools does that part in the editor module.
    """
    data = create_at("/%s" % plugin_name, plugin_name, unreal.GameFeatureData)
    if data is None:
        return None

    actor_paths = [actor for actor, _ in bindings]
    component_paths = [component for _, component in bindings]

    if not unreal.NightfallContentTools.configure_add_components_action(
            data, actor_paths, component_paths):
        unreal.log_error("could not configure actions for %s" % plugin_name)
        return None

    save(data)
    return data


def build_feature_data():
    banner("game feature data")

    grid = _build_feature_data("GridRestoration", [
        # One action, four classes. This is the whole of the loop's coupling to the game.
        (CORE + "NightfallRelayPylon", GRID + "GridNodeComponent"),
        (CORE + "NightfallPhysicsProp", GRID + "GridCellComponent"),
        (CORE + "NightfallSentinelDrone", GRID + "GridAlarmComponent"),
        (CORE + "NightfallCharacter", GRID + "GridHudComponent"),
    ])

    scanner = _build_feature_data("SurveyScanner", [
        (CORE + "NightfallCharacter", SCANNER + "ScannerComponent"),
    ])

    return {"GridRestoration": grid, "SurveyScanner": scanner}


# --- entry points --------------------------------------------------------------------------

def build_all(materials, meshes):
    banner("data")

    lut = build_lut()
    result = {
        "input": build_input(),
        "scanner_input": build_scanner_input(),
        "lut": lut,
        "sky": build_sky_profile(lut),
        "profiles": build_machine_profiles(materials, meshes),
        "tuning": build_tuning_table(),
        "features": build_feature_data(),
    }
    return result


def load_all():
    """Reload the data stage's output when it was skipped."""
    return {
        "input": load("input", "IC_NF_Default"),
        "scanner_input": unreal.EditorAssetLibrary.load_asset("/SurveyScanner/Input/IC_Scanner"),
        "lut": load("textures", "T_NF_Grade"),
        "sky": load("data", "DA_NF_SkyProfile"),
        "profiles": {
            "pylon": load("data", "DA_NF_PylonProfile"),
            "drone": load("data", "DA_NF_DroneProfile"),
            "door": load("data", "DA_NF_DoorProfile"),
        },
        "tuning": load("data", "DT_NF_SentinelTuning"),
        "features": None,
    }
