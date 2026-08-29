"""Assemble the shipped region.

Everything is placed by arithmetic against the same terrain height function the meshes were
built from, so nothing needs a trace and nothing floats. The layout is a compound in the
middle of a basin with six relay pylons ringing it, which gives the loop a hub and six
spokes rather than a checklist.

Data layers split the world three ways - Terrain, Structures, Machines - so streaming and
visibility can be reasoned about per category.
"""

import math
import os
import shutil

import unreal

import nf_meshes
from nf_common import PATHS, banner, create, linear, rot, save, vec
from nf_meshes import REGION_HALF, TERRAIN_TILES, terrain_height, tile_center

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

MAP_NAME = "L_RelayField"
TEMPLATE = "/Engine/Maps/Templates/OpenWorld"

#: Where the six relay pylons stand, in world XY.
PYLON_SITES = [
    (-14200.0, -12400.0),
    (-3200.0, -19800.0),
    (14800.0, -9400.0),
    (16200.0, 8600.0),
    (-1400.0, 17400.0),
    (-17600.0, 6200.0),
]

#: Where the player starts: an outer corner, looking in at the field.
PLAYER_START = (-21000.0, -21000.0)

#: Where the filament beds go. Grass binds the ground, so these stay clean.
FILAMENT_SITES = [(0.0, 6200.0)] + [(x * 0.62, y * 0.62) for x, y in PYLON_SITES]

#: Ground that must never read as dust: the levelled compound, then every filament bed.
#: (centre x, centre y, half x, half y) in cm. The half extents carry a margin over the
#: real footprint so haze thins out before the surface changes rather than at the seam.
CLEAN_ZONES = [(0.0, 0.0, 6400.0, 6400.0)]
CLEAN_ZONES += [(x, y, 3200.0, 3200.0) for x, y in FILAMENT_SITES]


# --- placement helpers -------------------------------------------------------------------

def ground(x, y, lift=0.0):
    return vec(x, y, terrain_height(x, y) + lift)


def spawn(actor_class, location, rotation=None, label=None):
    actor = EAS.spawn_actor_from_class(actor_class, location, rotation or rot())
    if actor and label:
        actor.set_actor_label(label, False)
    return actor


def place_mesh(mesh, x, y, yaw=0.0, lift=0.0, scale=1.0, label=None, material=None):
    """Drop a static mesh onto the ground at a world position."""
    actor = spawn(unreal.StaticMeshActor, ground(x, y, lift), rot(yaw=yaw), label)
    if actor is None:
        return None

    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    if material is not None:
        component.set_material(0, material)
    if scale != 1.0:
        actor.set_actor_scale3d(vec(scale, scale, scale))
    return actor


# --- deterministic dressing ---------------------------------------------------------------

def scatter(rng_seed, count, center, radius, place_fn, min_spacing=0.0):
    """Poisson-ish scatter around a point, reproducible from a seed.

    Rejection against already accepted points keeps debris from stacking. The stream is a
    plain hash of the index, so the same seed always yields the same field.
    """
    accepted = []
    attempts = 0
    while len(accepted) < count and attempts < count * 30:
        index = attempts
        attempts += 1

        angle = nf_meshes._hash2(index, 1, rng_seed) * math.tau
        distance = math.sqrt(nf_meshes._hash2(index, 2, rng_seed)) * radius
        x = center[0] + math.cos(angle) * distance
        y = center[1] + math.sin(angle) * distance

        if abs(x) > REGION_HALF - 800.0 or abs(y) > REGION_HALF - 800.0:
            continue
        if any((x - ax) ** 2 + (y - ay) ** 2 < min_spacing ** 2 for ax, ay in accepted):
            continue

        accepted.append((x, y))
        place_fn(index, x, y)

    return len(accepted)


# --- data layers ----------------------------------------------------------------------------

def build_data_layers():
    """Three runtime data layers, created as assets and instanced into the world."""
    subsystem = unreal.get_editor_subsystem(unreal.DataLayerEditorSubsystem)
    layers = {}

    # DebugColor is an FColor, so these are 0-255 rather than linear.
    definitions = [
        ("DL_Terrain", unreal.Color(r=92, g=78, b=66, a=255)),
        ("DL_Structures", unreal.Color(r=78, g=110, b=143, a=255)),
        ("DL_Machines", unreal.Color(r=255, g=160, b=48, a=255)),
    ]

    for name, color in definitions:
        asset = create("data", name, unreal.DataLayerAsset)
        if asset is None:
            continue
        asset.set_editor_property("debug_color", color)
        # Runtime layers can be loaded and unloaded while playing, which is the point of
        # splitting the world up this way.
        asset.set_editor_property("data_layer_type", unreal.DataLayerType.RUNTIME)
        save(asset)

        params = unreal.DataLayerCreationParameters()
        params.set_editor_property("data_layer_asset", asset)
        instance = subsystem.create_data_layer_instance(params)
        if instance is None:
            unreal.log_error("could not create a data layer instance for %s" % name)
            continue

        # Runtime data layers start Unloaded unless told otherwise, which would leave the
        # whole world invisible at play. Activated is the right default here: the layers
        # exist so systems can switch them off deliberately, not so the world starts empty.
        instance.set_editor_property("initial_runtime_state",
                                     unreal.DataLayerRuntimeState.ACTIVATED)
        layers[name] = instance

    unreal.log("data layers: %d" % len(layers))
    return subsystem, layers


def assign(subsystem, layers, layer_name, actors):
    instance = layers.get(layer_name)
    if instance is None:
        return 0

    live = [a for a in actors if a is not None]
    if not live:
        return 0

    subsystem.add_actors_to_data_layer(live, instance)
    return len(live)


# --- the build ---------------------------------------------------------------------------------

def build(materials, meshes, data):
    banner("level")

    map_path = "%s/%s" % (PATHS["maps"], MAP_NAME)
    clear_existing_map(map_path)

    if not LES.new_level_from_template(map_path, TEMPLATE):
        unreal.log_error("could not create the level from %s" % TEMPLATE)
        return

    strip_template_actors()

    subsystem, layers = build_data_layers()

    terrain_actors = place_terrain(meshes)
    structure_actors = place_structures(meshes, materials)
    machine_actors = place_machines(meshes, materials, data)
    dressing_actors = place_dressing(meshes, materials)
    dust_actors = place_dust()
    place_sky(data)
    place_player_start()
    place_pcg(meshes)

    assign(subsystem, layers, "DL_Terrain", terrain_actors + dust_actors)
    assign(subsystem, layers, "DL_Structures", structure_actors + dressing_actors)
    assign(subsystem, layers, "DL_Machines", machine_actors)

    LES.save_current_level()
    unreal.log("level saved: %s" % map_path)


def clear_existing_map(map_path):
    """Remove a previous build's map so the level stage is re-runnable.

    One file per actor means the map is a folder of loose packages as well as a .umap, and
    new_level_from_template refuses to overwrite. Both have to go.
    """
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        unreal.EditorAssetLibrary.delete_asset(map_path)
        unreal.SystemLibrary.collect_garbage()

    content_dir = unreal.Paths.project_content_dir()
    relative = PATHS["maps"].replace("/Game/", "")

    for folder in ("__ExternalActors__", "__ExternalObjects__"):
        path = os.path.abspath(os.path.join(content_dir, relative, folder, MAP_NAME))
        if os.path.isdir(path):
            shutil.rmtree(path, ignore_errors=True)
            unreal.log("removed %s" % path)

    # The HLOD holder is regenerated with the map.
    hlod = "%s/%s_HLOD0_Instancing" % (PATHS["maps"], MAP_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(hlod):
        unreal.EditorAssetLibrary.delete_asset(hlod)


def strip_template_actors():
    """Remove the template's own sky and landscape; this project brings its own.

    Only classes we positively replace are removed. World partition bookkeeping actors and
    anything unrecognised are left alone.
    """
    removable = (
        unreal.Landscape, unreal.LandscapeStreamingProxy, unreal.LandscapeProxy,
        unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere,
        unreal.ExponentialHeightFog, unreal.VolumetricCloud, unreal.PostProcessVolume,
        unreal.PlayerStart, unreal.StaticMeshActor,
    )

    removed = 0
    for actor in EAS.get_all_level_actors():
        if isinstance(actor, removable):
            EAS.destroy_actor(actor)
            removed += 1

    unreal.log("stripped %d template actors" % removed)


def place_terrain(meshes):
    actors = []
    for tile_y in range(TERRAIN_TILES):
        for tile_x in range(TERRAIN_TILES):
            mesh = meshes["terrain"].get((tile_x, tile_y))
            if mesh is None:
                continue
            center_x, center_y = tile_center(tile_x, tile_y)
            actor = spawn(unreal.StaticMeshActor, vec(center_x, center_y, 0.0), rot(),
                          "Terrain_%d_%d" % (tile_x, tile_y))
            component = actor.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_mobility(unreal.ComponentMobility.STATIC)
            # Bare basalt: this is the ground a rotor can lift. Structures and the compound
            # platform go untagged, so a drone hovering over them raises nothing.
            actor.tags = ["NF_Dusty"]
            actors.append(actor)

    unreal.log("terrain actors: %d" % len(actors))
    return actors


def place_structures(meshes, materials):
    """A compound in the middle, and a small outpost at every pylon."""
    parts = meshes["structures"]
    actors = []

    # --- central compound ---------------------------------------------------------------
    actors.append(place_mesh(parts["Platform"], 0.0, 0.0, label="Compound_Platform"))
    actors.append(place_mesh(parts["Bunker"], 3400.0, -1600.0, yaw=-24.0, label="Compound_Bunker"))
    actors.append(place_mesh(parts["Tank"], -3800.0, 2900.0, label="Compound_Tank"))
    actors.append(place_mesh(parts["Tank"], -5100.0, 1200.0, label="Compound_Tank_B"))
    actors.append(place_mesh(parts["Catwalk"], -1900.0, -2600.0, yaw=64.0, lift=420.0,
                             label="Compound_Catwalk"))
    actors.append(place_mesh(parts["Stairs"], -1900.0, -3600.0, yaw=90.0, label="Compound_Stairs"))

    for index in range(6):
        angle = math.tau * index / 6.0
        actors.append(place_mesh(parts["Pillar"],
                                 math.cos(angle) * 2100.0, math.sin(angle) * 2100.0,
                                 label="Compound_Pillar_%d" % index))

    for index, (x, y, yaw) in enumerate([
        (-2400.0, 4200.0, 0.0), (4600.0, 3100.0, 68.0),
        (2200.0, -5200.0, 14.0), (-5400.0, -3400.0, 104.0),
    ]):
        actors.append(place_mesh(parts["Wall"], x, y, yaw=yaw, label="Compound_Wall_%d" % index))

    # --- outposts -------------------------------------------------------------------------
    for index, (site_x, site_y) in enumerate(PYLON_SITES):
        # Face each outpost's furniture back toward the compound.
        yaw = math.degrees(math.atan2(-site_y, -site_x))

        actors.append(place_mesh(parts["Platform"], site_x + 1500.0, site_y + 900.0, yaw=yaw,
                                 scale=0.7, label="Outpost%d_Platform" % index))
        actors.append(place_mesh(parts["Wall"], site_x - 1800.0, site_y + 1400.0, yaw=yaw + 90.0,
                                 scale=0.8, label="Outpost%d_Wall" % index))
        actors.append(place_mesh(parts["Pillar"], site_x - 900.0, site_y - 1500.0, scale=0.75,
                                 label="Outpost%d_Pillar" % index))
        if index % 2 == 0:
            actors.append(place_mesh(parts["Catwalk"], site_x + 2600.0, site_y - 1100.0,
                                     yaw=yaw + 30.0, lift=300.0, scale=0.8,
                                     label="Outpost%d_Catwalk" % index))

    actors = [a for a in actors if a is not None]
    unreal.log("structure actors: %d" % len(actors))
    return actors


def place_machines(meshes, materials, data):
    """Pylons, drones, doors and power cells."""
    actors = []
    profiles = data["profiles"]
    tuning = data["tuning"]

    # --- relay pylons ----------------------------------------------------------------------
    for index, (x, y) in enumerate(PYLON_SITES):
        pylon = spawn(unreal.NightfallRelayPylon, ground(x, y), rot(yaw=index * 37.0),
                      "Pylon_%d" % index)
        if pylon is None:
            continue
        pylon.set_editor_property("profile", profiles["pylon"])
        actors.append(pylon)

    # --- sentinels -------------------------------------------------------------------------
    # Each drone patrols between two pylon sites, so the routes cover the ground the player
    # has to cross rather than circling empty terrain.
    routes = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 0)]
    presets = ["Patrol", "Watchtower", "Patrol", "Hunter", "Patrol", "Watchtower"]

    for index, (from_site, to_site) in enumerate(routes):
        start = PYLON_SITES[from_site]
        finish = PYLON_SITES[to_site]
        mid = ((start[0] + finish[0]) * 0.5, (start[1] + finish[1]) * 0.5)

        drone = spawn(unreal.NightfallSentinelDrone, ground(start[0], start[1], 900.0),
                      rot(), "Sentinel_%d" % index)
        if drone is None:
            continue

        drone.set_editor_property("profile", profiles["drone"])
        if tuning is not None:
            drone.set_editor_property("tuning_table", tuning)
            drone.set_editor_property("tuning_row_name", presets[index])

        # Offsets are in the drone's own space, and it spawns unrotated, so these are
        # simply deltas from its start.
        drone.set_editor_property("patrol_offsets", [
            vec(0.0, 0.0, 0.0),
            vec(mid[0] - start[0], mid[1] - start[1],
                terrain_height(mid[0], mid[1]) - terrain_height(start[0], start[1]) + 260.0),
            vec(finish[0] - start[0], finish[1] - start[1],
                terrain_height(finish[0], finish[1]) - terrain_height(start[0], start[1])),
        ])
        actors.append(drone)

    # --- blast doors --------------------------------------------------------------------------
    for index, (x, y, yaw) in enumerate([
        (2560.0, -1600.0, -24.0),
        (PYLON_SITES[2][0] + 1500.0, PYLON_SITES[2][1] + 260.0, 90.0),
        (PYLON_SITES[4][0] - 1500.0, PYLON_SITES[4][1] - 260.0, -90.0),
    ]):
        door = spawn(unreal.NightfallBlastDoor, ground(x, y), rot(yaw=yaw), "BlastDoor_%d" % index)
        if door is None:
            continue
        door.set_editor_property("profile", profiles["door"])
        actors.append(door)

    # --- power cells ---------------------------------------------------------------------------
    # Two cells fill a pylon and there are six pylons, so twelve are needed. Sixteen are
    # placed: the surplus is slack for cells knocked somewhere awkward.
    # The tag is declared by the GridRestoration plugin. Placing it here is the whole
    # handshake: the level says "this prop is a power cell" and the feature decides what
    # that means.
    cell_tag = _tag_library.make_gameplay_tag_container_from_tag(_tag("Nightfall.Grid.PowerCell"))

    cell_sites = [(0.0, 0.0)] + list(PYLON_SITES)
    placed = 0
    for site_index, (site_x, site_y) in enumerate(cell_sites):
        wanted = 4 if site_index == 0 else 2
        for slot in range(wanted):
            angle = math.tau * (slot + site_index * 0.37) / max(wanted, 1)
            x = site_x + math.cos(angle) * 1150.0
            y = site_y + math.sin(angle) * 1150.0

            cell = spawn(unreal.NightfallPhysicsProp, ground(x, y, 90.0), rot(yaw=angle * 57.3),
                         "PowerCell_%d_%d" % (site_index, slot))
            if cell is None:
                continue
            cell.set_editor_property("prop_mesh", meshes["props"]["PowerCell"])
            cell.set_editor_property("prop_material", materials["CellShell"])
            cell.set_editor_property("prop_tags", cell_tag)
            cell.set_editor_property("mass_kg", 26.0)
            cell.set_editor_property("glow_color", linear(0.16, 0.86, 1.0))
            cell.set_editor_property("glow_intensity", 950.0)
            actors.append(cell)
            placed += 1

    unreal.log("machines: %d pylons, %d sentinels, %d doors, %d cells"
               % (len(PYLON_SITES), len(routes), 3, placed))
    return [a for a in actors if a is not None]


_tag_library = getattr(unreal, "GameplayTagLibrary", None) or getattr(unreal, "BlueprintGameplayTagLibrary", None)


def _tag(tag_name):
    """Resolve a registered gameplay tag by name."""
    tag = unreal.GameplayTag()
    tag.import_text(tag_name)
    if not _tag_library.is_gameplay_tag_valid(tag):
        raise RuntimeError("gameplay tag '%s' is not registered" % tag_name)
    return tag


def place_dressing(meshes, materials):
    """Rubble and filament fields.

    The scatter is a seeded hash rather than a random stream, so the field is identical on
    every machine and can be rebuilt rather than stored.
    """
    actors = []
    rubble = [meshes["props"]["Rubble0"], meshes["props"]["Rubble1"]]

    def place_rubble(index, x, y):
        mesh = rubble[index % len(rubble)]
        actor = place_mesh(mesh, x, y,
                           yaw=nf_meshes._hash2(index, 7, 991) * 360.0,
                           scale=0.7 + nf_meshes._hash2(index, 8, 991) * 1.5,
                           label="Rubble_%d" % index,
                           material=materials["Basalt"])
        if actor is not None:
            actors.append(actor)

    total = scatter(991, 90, (0.0, 0.0), REGION_HALF * 0.92, place_rubble, min_spacing=900.0)

    # --- filament fields ---------------------------------------------------------------------
    field_sites = FILAMENT_SITES
    for index, (x, y) in enumerate(field_sites):
        field = spawn(unreal.NightfallFilamentField, ground(x, y), rot(),
                      "FilamentField_%d" % index)
        if field is None:
            continue
        field.set_editor_property("filament_mesh", meshes["filament"])
        field.set_editor_property("seed", 20261 + index * 17)
        field.set_editor_property("instance_count", 2400)
        field.set_editor_property("extent", unreal.Vector2D(2600.0, 2600.0))
        field.set_editor_property("wind_bearing_degrees", 35.0 + index * 11.0)
        actors.append(field)

    unreal.log("dressing: %d rubble, %d filament fields" % (total, len(field_sites)))
    return [a for a in actors if a is not None]


def place_dust():
    """Ground haze over bare basalt, and nowhere else.

    Local fog volumes rather than particles: this project has no Niagara, and a sphere of
    height fog sitting on the ground is what dust hanging in still air actually is. They
    are placed only outside the clean zones, so the compound reads as swept pavement and
    the filament beds as bound ground while the open field reads as dust.
    """
    actors = []

    def is_clean(x, y):
        return any(abs(x - cx) <= hx and abs(y - cy) <= hy
                   for cx, cy, hx, hy in CLEAN_ZONES)

    def place_one(index, x, y):
        if is_clean(x, y):
            return

        volume = spawn(unreal.LocalFogVolume, ground(x, y), rot(), "DustHaze_%d" % index)
        if volume is None:
            return

        # The volume is always a sphere of 500 cm times the largest axis scale, so 26 gives
        # a 130 m radius and the neighbours overlap into one continuous layer.
        volume.set_actor_scale3d(vec(26.0, 26.0, 26.0))

        fog = volume.get_component_by_class(unreal.LocalFogVolumeComponent)
        if fog is None:
            return

        # Radial extinction only softens the sphere's edge - push it and the haze reads as
        # a visible ball. The height term is what makes it a layer on the ground.
        fog.set_editor_property("radial_fog_extinction", 0.05)
        fog.set_editor_property("height_fog_extinction", 0.34)
        # Falloff is in normalised volume units, not cm: (500 * 26) / (2600 * 0.01) = 500 cm
        # of e-fold thickness, which is roughly the height a person walks through.
        fog.set_editor_property("height_fog_falloff", 2600.0)
        fog.set_editor_property("height_fog_offset", 0.0)
        # Forward scattering, so the dust lights up when the low sun rakes through it.
        fog.set_editor_property("fog_phase_g", 0.55)
        # Warm and dark: dust is not water vapour and must not read as white.
        fog.set_editor_property("fog_albedo", linear(0.62, 0.55, 0.46))
        fog.set_editor_property("fog_emissive", linear(0.0, 0.0, 0.0))
        fog.set_editor_property("fog_sort_priority", 0)
        actors.append(volume)

    scatter(4801, 26, (0.0, 0.0), REGION_HALF * 0.86, place_one, min_spacing=6000.0)
    unreal.log("dust: %d haze volumes" % len(actors))
    return actors


def place_sky(data):
    director = spawn(unreal.NightfallSkyDirector, vec(0.0, 0.0, 4000.0), rot(), "SkyDirector")
    if director is not None and data.get("sky") is not None:
        director.set_editor_property("sky_profile", data["sky"])
    return director


def place_player_start():
    x, y = PLAYER_START
    # Face the compound.
    yaw = math.degrees(math.atan2(-y, -x))
    return spawn(unreal.PlayerStart, ground(x, y, 140.0), rot(yaw=yaw), "PlayerStart")


def place_pcg(meshes):
    """A PCG volume that scatters debris across the compound floor.

    Kept to the flat ground around the compound on purpose: the graph lays a seeded grid on
    the volume's own floor plane rather than following the terrain. Over the compound's
    levelled ground that is exactly right.
    """
    graph = create("pcg", "PCG_NF_Debris", unreal.PCGGraph)
    if graph is None:
        return None

    # A PCGVolume's brush is a 200 cm cube, so the scale below gives this half extent. The
    # graph needs it because the grid is generated from extents and then culled.
    volume_scale = 58.0
    half_extent = 100.0 * volume_scale

    ok = unreal.NightfallContentTools.build_debris_scatter_graph(
        graph, [meshes["props"]["Rubble0"], meshes["props"]["Rubble1"]],
        0.0025, half_extent, 6180)
    if not ok:
        unreal.log_error("PCG debris graph could not be built")
        return None
    save(graph)

    # Centred on the ground, not above it: the grid is a single layer at the volume's own
    # local z=0, and rubble pivots sit at the base of the mesh.
    volume = spawn(unreal.PCGVolume, ground(0.0, 0.0, 0.0), rot(), "PCG_Debris")
    if volume is None:
        return None

    volume.set_actor_scale3d(vec(volume_scale, volume_scale, 3.0))
    component = volume.get_component_by_class(unreal.PCGComponent)
    if component is None:
        unreal.log_error("PCG volume has no PCG component")
        return volume

    # Graph is a protected property with a public setter; the seed lives on the graph's
    # own nodes, set when the graph was built.
    component.set_graph(graph)

    # Deliberately not generated here. The component generates on load, and kicking off an
    # async generation during the build leaves it in flight while the level is being
    # written out.
    unreal.log("PCG volume placed")
    return volume
