"""Every mesh in the project, generated from primitives and arithmetic.

Nothing here is imported. Terrain is a deterministic height function sampled on a grid;
machines are boxes, cylinders and cones stacked into rigid hierarchies; rings are rings of
boxes rather than tori, because a faceted ring is both cheaper and closer to the look.

Two properties are load bearing:

  * Terrain is generated in world space and then translated back to the tile's origin, so
    neighbouring tiles share exact vertex positions along their seam. No stitching pass.
  * The same height function is importable, so the level build can drop a pylon on the
    ground by asking for the height at its coordinates rather than by tracing.
"""

import math

import unreal

from nf_common import PATHS, banner, save

GS_PRIM = unreal.GeometryScript_Primitives
GS_QUERY = unreal.GeometryScript_MeshQueries
GS_EDITS = unreal.GeometryScript_MeshEdits
GS_XFORM = unreal.GeometryScript_MeshTransforms
GS_ASSETS = unreal.GeometryScript_NewAssetUtils

def _enum_value(enum_type, *candidates):
    """Pick the first name an engine enum actually exposes.

    Engine enum spellings move between releases; naming the alternatives here beats
    failing at import time on a one word difference.
    """
    for name in candidates:
        if hasattr(enum_type, name):
            return getattr(enum_type, name)
    raise AttributeError("%s exposes none of %s; it has %s"
                         % (enum_type, candidates,
                            [n for n in dir(enum_type) if n.isupper()]))


ORIGIN_BASE = _enum_value(unreal.GeometryScriptPrimitiveOriginMode, "BASE")
ORIGIN_CENTER = _enum_value(unreal.GeometryScriptPrimitiveOriginMode, "CENTER", "CENTERED", "CENTRE")

# --- region layout ---------------------------------------------------------------------

#: Half the width of the shipped region, in cm. 512 m across.
REGION_HALF = 25600.0

#: Tiles per side of the terrain grid.
TERRAIN_TILES = 4

#: Quads per side of one terrain tile.
TERRAIN_STEPS = 48

#: Height of one filament blade in cm. Must match the material's BladeHeight parameter.
BLADE_HEIGHT = 180.0


# --- deterministic noise ------------------------------------------------------------------

def _hash2(ix, iy, seed):
    """Integer hash to a float in [0,1). Stable across runs and machines."""
    h = (ix * 374761393 + iy * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) & 0xFFFFFFFF
    h = (h * 1274126177) & 0xFFFFFFFF
    h = (h ^ (h >> 16)) & 0xFFFFFFFF
    return h / 4294967296.0


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, seed):
    """Smoothed lattice noise in [0,1)."""
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    sx, sy = _smooth(fx), _smooth(fy)

    n00 = _hash2(ix, iy, seed)
    n10 = _hash2(ix + 1, iy, seed)
    n01 = _hash2(ix, iy + 1, seed)
    n11 = _hash2(ix + 1, iy + 1, seed)

    top = n00 + (n10 - n00) * sx
    bottom = n01 + (n11 - n01) * sx
    return top + (bottom - top) * sy


def fbm(x, y, seed, octaves=3):
    total, amplitude, frequency, norm = 0.0, 1.0, 1.0, 0.0
    for octave in range(octaves):
        total += amplitude * value_noise(x * frequency, y * frequency, seed + octave * 101)
        norm += amplitude
        amplitude *= 0.5
        frequency *= 2.03
    return total / norm


def terrain_height(wx, wy):
    """Ground height in cm at a world position.

    Three octaves of rolling basalt, plus a shallow bowl that lifts the horizon at the
    edge of the region. The bowl is what stops the shipped 512 m reading as a square cut
    out of nothing: the player sees ground rising in every direction instead of an edge.
    """
    height = 240.0 * (fbm(wx / 11000.0, wy / 11000.0, 11) - 0.5)
    height += 95.0 * (fbm(wx / 3600.0, wy / 3600.0, 23) - 0.5)
    height += 28.0 * (fbm(wx / 1100.0, wy / 1100.0, 37) - 0.5)

    radial = (wx * wx + wy * wy) / (REGION_HALF * REGION_HALF)
    height += 560.0 * radial * radial

    return height


# --- mesh helpers ---------------------------------------------------------------------------

def new_mesh():
    return unreal.new_object(unreal.DynamicMesh)


def options(material_id=0):
    opts = unreal.GeometryScriptPrimitiveOptions()
    opts.set_editor_property("material_id", material_id)
    return opts


def xf(x=0.0, y=0.0, z=0.0, pitch=0.0, yaw=0.0, roll=0.0, scale=1.0):
    """Transform helper.

    Rotator components are passed by keyword because unreal.Rotator's positional order is
    (roll, pitch, yaw), which silently puts a yaw onto pitch if you assume otherwise.
    """
    return unreal.Transform(
        unreal.Vector(x, y, z),
        unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
        unreal.Vector(scale, scale, scale))


def box(mesh, size_x, size_y, size_z, transform=None, origin=ORIGIN_CENTER, material_id=0):
    GS_PRIM.append_box(mesh, options(material_id), transform or xf(),
                       size_x, size_y, size_z, 0, 0, 0, origin)
    return mesh


def cylinder(mesh, radius, height, sides=12, transform=None, origin=ORIGIN_BASE, material_id=0):
    GS_PRIM.append_cylinder(mesh, options(material_id), transform or xf(),
                            radius, height, sides, 0, True, origin)
    return mesh


def cone(mesh, base_radius, top_radius, height, sides=12, transform=None,
         origin=ORIGIN_BASE, material_id=0):
    GS_PRIM.append_cone(mesh, options(material_id), transform or xf(),
                        base_radius, top_radius, height, sides, 0, True, origin)
    return mesh


def sphere(mesh, radius, steps=6, transform=None, material_id=0):
    GS_PRIM.append_sphere_box(mesh, options(material_id), transform or xf(),
                              radius, steps, steps, steps, ORIGIN_CENTER)
    return mesh


def box_ring(mesh, radius, count, segment_thickness, segment_height, z=0.0, material_id=0):
    """A ring built from box segments.

    Faceted on purpose. A twelve segment ring is 144 triangles, reads as machined, and
    needs no torus primitive.
    """
    segment_length = (2.0 * math.pi * radius / count) * 1.08
    for index in range(count):
        angle = 2.0 * math.pi * index / count
        # The segment's length is its local X, and a ring needs that running tangentially.
        # Facing it along the radius instead turns the ring into a set of spokes, which is
        # exactly what the first build produced.
        transform = xf(radius * math.cos(angle), radius * math.sin(angle), z,
                       yaw=math.degrees(angle) + 90.0)
        box(mesh, segment_length, segment_thickness, segment_height, transform,
            ORIGIN_CENTER, material_id)
    return mesh


def vertex_positions(mesh):
    """Positions as a (list_struct, python_list) pair.

    The struct shares its storage with the mesh, so writing through set_vector_list_item
    and handing the same struct back is the cheap way to displace a grid.
    """
    result = GS_QUERY.get_all_vertex_positions(mesh, True)
    vector_list = result[1] if isinstance(result, tuple) else result
    return vector_list, vector_list.convert_vector_list_to_array()


# --- asset creation ------------------------------------------------------------------------

_static_mesh_subsystem = None


def _collision_subsystem():
    global _static_mesh_subsystem
    if _static_mesh_subsystem is None and hasattr(unreal, "StaticMeshEditorSubsystem"):
        _static_mesh_subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    return _static_mesh_subsystem


def commit(mesh, name, material=None, simple_collision=False):
    """Bake a dynamic mesh into a StaticMesh asset.

    Static geometry uses complex-as-simple collision, which costs nothing to author and is
    exact. Anything that will simulate needs a real simple primitive instead, because Chaos
    cannot build a rigid body from a triangle soup.
    """
    path = "%s/%s" % (PATHS["meshes"], name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)

    asset_options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    asset_options.set_editor_property("enable_recompute_normals", True)
    asset_options.set_editor_property("enable_recompute_tangents", True)
    asset_options.set_editor_property("enable_nanite", False)

    result = GS_ASSETS.create_new_static_mesh_asset_from_mesh(mesh, path, asset_options)
    static_mesh = result[0] if isinstance(result, tuple) else result
    if static_mesh is None:
        unreal.log_error("failed to create static mesh %s" % path)
        return None

    if material is not None:
        static_mesh.set_editor_property(
            "static_materials",
            [unreal.StaticMaterial(material_interface=material,
                                   material_slot_name="Surface")])

    if simple_collision:
        subsystem = _collision_subsystem()
        shape_enum = getattr(unreal, "ScriptCollisionShapeType", None) or             getattr(unreal, "ScriptingCollisionShapeType", None)
        if subsystem is not None and shape_enum is not None:
            subsystem.add_simple_collisions(static_mesh, _enum_value(shape_enum, "BOX"))
        else:
            unreal.log_error("no collision authoring API; %s has no simple collision" % name)
    else:
        body_setup = static_mesh.get_editor_property("body_setup")
        if body_setup is not None:
            body_setup.set_editor_property(
                "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)

    save(static_mesh)
    return static_mesh


# --- terrain -----------------------------------------------------------------------------

def tile_center(tile_x, tile_y):
    """World centre of a terrain tile."""
    tile_size = (REGION_HALF * 2.0) / TERRAIN_TILES
    return (-REGION_HALF + tile_size * (tile_x + 0.5),
            -REGION_HALF + tile_size * (tile_y + 0.5))


def build_terrain(material):
    """One static mesh per tile, so the region streams as sixteen pieces."""
    tile_size = (REGION_HALF * 2.0) / TERRAIN_TILES
    meshes = {}

    for tile_y in range(TERRAIN_TILES):
        for tile_x in range(TERRAIN_TILES):
            center_x, center_y = tile_center(tile_x, tile_y)

            mesh = new_mesh()
            # Build the grid at its world position so the noise is sampled in world space
            # and adjacent tiles agree exactly along their shared edge.
            GS_PRIM.append_rectangle_xy(
                mesh, options(), xf(center_x, center_y, 0.0),
                tile_size, tile_size, TERRAIN_STEPS, TERRAIN_STEPS)

            vector_list, positions = vertex_positions(mesh)
            for index, position in enumerate(positions):
                vector_list.set_vector_list_item(
                    index,
                    unreal.Vector(position.x, position.y,
                                  terrain_height(position.x, position.y)))
            GS_EDITS.set_all_mesh_vertex_positions(mesh, vector_list)

            # Recentre so the asset's bounds are tight around its own tile.
            GS_XFORM.translate_mesh(mesh, unreal.Vector(-center_x, -center_y, 0.0))

            name = "SM_NF_Terrain_%d_%d" % (tile_x, tile_y)
            meshes[(tile_x, tile_y)] = commit(mesh, name, material)

    unreal.log("terrain: %d tiles, %d triangles each"
               % (len(meshes), TERRAIN_STEPS * TERRAIN_STEPS * 2))
    return meshes


# --- pylon ---------------------------------------------------------------------------------

def build_pylon(materials):
    parts = {}

    mesh = new_mesh()
    cylinder(mesh, 300.0, 70.0, 6, xf(), ORIGIN_BASE)
    cylinder(mesh, 210.0, 130.0, 6, xf(z=70.0), ORIGIN_BASE)
    # Three buttresses tie the plinth to the ground and break the silhouette.
    for index in range(3):
        angle = 2.0 * math.pi * index / 3.0
        box(mesh, 320.0, 70.0, 46.0,
            xf(190.0 * math.cos(angle), 190.0 * math.sin(angle), 23.0,
               yaw=math.degrees(angle)), ORIGIN_CENTER)
    parts["Base"] = commit(mesh, "SM_NF_PylonBase", materials["Concrete"])

    mesh = new_mesh()
    box_ring(mesh, 250.0, 10, 54.0, 44.0)
    parts["Collar"] = commit(mesh, "SM_NF_PylonCollar", materials["SteelDark"])

    mesh = new_mesh()
    cone(mesh, 62.0, 40.0, 560.0, 8, xf(), ORIGIN_BASE)
    # Fins run the length of the mast so the telescoping travel reads clearly.
    for index in range(4):
        angle = 2.0 * math.pi * index / 4.0
        box(mesh, 26.0, 14.0, 520.0,
            xf(52.0 * math.cos(angle), 52.0 * math.sin(angle), 280.0,
               yaw=math.degrees(angle)), ORIGIN_CENTER)
    parts["Mast"] = commit(mesh, "SM_NF_PylonMast", materials["Steel"])

    mesh = new_mesh()
    box_ring(mesh, 185.0, 12, 30.0, 26.0)
    parts["RingLower"] = commit(mesh, "SM_NF_PylonRingLower", materials["SteelWorn"])

    mesh = new_mesh()
    box_ring(mesh, 132.0, 9, 26.0, 22.0)
    parts["RingUpper"] = commit(mesh, "SM_NF_PylonRingUpper", materials["SteelWorn"])

    mesh = new_mesh()
    sphere(mesh, 76.0, 5)
    parts["Core"] = commit(mesh, "SM_NF_PylonCore", materials["PanelAmber"])

    return parts


# --- sentinel drone ----------------------------------------------------------------------

def build_drone(materials):
    parts = {}

    mesh = new_mesh()
    box(mesh, 190.0, 190.0, 44.0, xf(), ORIGIN_CENTER)
    cone(mesh, 130.0, 78.0, 40.0, 8, xf(z=22.0), ORIGIN_BASE)
    cone(mesh, 130.0, 90.0, 30.0, 8, xf(z=-22.0, pitch=180.0), ORIGIN_BASE)
    # Four arms out to the rotors.
    for index in range(4):
        angle = math.pi / 4.0 + 2.0 * math.pi * index / 4.0
        box(mesh, 190.0, 34.0, 22.0,
            xf(118.0 * math.cos(angle), 118.0 * math.sin(angle), 0.0,
               yaw=math.degrees(angle)), ORIGIN_CENTER)
    parts["Hull"] = commit(mesh, "SM_NF_DroneHull", materials["SteelDark"])

    mesh = new_mesh()
    box_ring(mesh, 58.0, 8, 20.0, 18.0)
    parts["YawRing"] = commit(mesh, "SM_NF_DroneYawRing", materials["Steel"])

    mesh = new_mesh()
    box(mesh, 34.0, 74.0, 34.0, xf(), ORIGIN_CENTER)
    box(mesh, 26.0, 26.0, 54.0, xf(z=-30.0), ORIGIN_CENTER)
    parts["PitchArm"] = commit(mesh, "SM_NF_DronePitchArm", materials["Steel"])

    mesh = new_mesh()
    cone(mesh, 46.0, 30.0, 58.0, 8, xf(z=-58.0), ORIGIN_BASE)
    sphere(mesh, 25.0, 4, xf(z=-64.0))
    parts["SensorPod"] = commit(mesh, "SM_NF_DroneSensorPod", materials["PanelCyan"])

    mesh = new_mesh()
    # A two-blade rotor. Spinning it fast is what sells the hover.
    box(mesh, 210.0, 22.0, 6.0, xf(), ORIGIN_CENTER)
    box(mesh, 22.0, 210.0, 6.0, xf(), ORIGIN_CENTER)
    cylinder(mesh, 18.0, 20.0, 8, xf(z=-8.0), ORIGIN_BASE)
    parts["Rotor"] = commit(mesh, "SM_NF_DroneRotor", materials["SteelWorn"])

    return parts


# --- blast door -----------------------------------------------------------------------------

def build_door(materials):
    parts = {}

    mesh = new_mesh()
    box(mesh, 90.0, 70.0, 520.0, xf(y=-250.0, z=260.0), ORIGIN_CENTER)
    box(mesh, 90.0, 70.0, 520.0, xf(y=250.0, z=260.0), ORIGIN_CENTER)
    box(mesh, 90.0, 570.0, 70.0, xf(z=485.0), ORIGIN_CENTER)
    box(mesh, 90.0, 570.0, 40.0, xf(z=20.0), ORIGIN_CENTER)
    parts["Frame"] = commit(mesh, "SM_NF_DoorFrame", materials["Concrete"])

    mesh = new_mesh()
    box(mesh, 46.0, 215.0, 420.0, xf(), ORIGIN_CENTER)
    # Ribs, so a moving leaf reads against the frame.
    for index in range(3):
        box(mesh, 54.0, 190.0, 22.0, xf(z=-120.0 + index * 120.0), ORIGIN_CENTER)
    parts["Leaf"] = commit(mesh, "SM_NF_DoorLeaf", materials["SteelDark"])

    mesh = new_mesh()
    box_ring(mesh, 62.0, 8, 20.0, 20.0)
    for index in range(4):
        angle = 2.0 * math.pi * index / 4.0
        box(mesh, 120.0, 16.0, 14.0, xf(yaw=math.degrees(angle)), ORIGIN_CENTER)
    parts["Wheel"] = commit(mesh, "SM_NF_DoorWheel", materials["SteelWorn"])

    mesh = new_mesh()
    box(mesh, 14.0, 90.0, 34.0, xf(), ORIGIN_CENTER)
    parts["Panel"] = commit(mesh, "SM_NF_DoorPanel", materials["PanelAmber"])

    return parts


# --- props ------------------------------------------------------------------------------------

def build_props(materials):
    parts = {}

    mesh = new_mesh()
    box(mesh, 62.0, 62.0, 96.0, xf(), ORIGIN_CENTER)
    box(mesh, 74.0, 74.0, 16.0, xf(z=40.0), ORIGIN_CENTER)
    box(mesh, 74.0, 74.0, 16.0, xf(z=-40.0), ORIGIN_CENTER)
    # A window down each side so the charge inside reads from any angle.
    for index in range(4):
        angle = 2.0 * math.pi * index / 4.0
        box(mesh, 30.0, 66.0, 52.0, xf(yaw=math.degrees(angle)), ORIGIN_CENTER)
    parts["PowerCell"] = commit(mesh, "SM_NF_PowerCell", materials["CellShell"],
                                simple_collision=True)

    for variant in range(2):
        mesh = new_mesh()
        seed = 400 + variant * 37
        for index in range(4):
            scale = 40.0 + 60.0 * _hash2(index, variant, seed)
            box(mesh, scale * 1.6, scale * 1.2, scale,
                xf(60.0 * (_hash2(index, 1, seed) - 0.5),
                   60.0 * (_hash2(index, 2, seed) - 0.5),
                   scale * 0.5,
                   pitch=20.0 * (_hash2(index, 3, seed) - 0.5),
                   yaw=360.0 * _hash2(index, 4, seed)),
                ORIGIN_CENTER)
        parts["Rubble%d" % variant] = commit(
            mesh, "SM_NF_Rubble%02d" % variant, materials["Basalt"])

    return parts


# --- world structures ---------------------------------------------------------------------------

def build_structures(materials):
    parts = {}

    mesh = new_mesh()
    box(mesh, 1600.0, 1600.0, 90.0, xf(), ORIGIN_BASE)
    box(mesh, 1400.0, 1400.0, 60.0, xf(z=-60.0), ORIGIN_BASE)
    parts["Platform"] = commit(mesh, "SM_NF_Platform", materials["Concrete"])

    mesh = new_mesh()
    box(mesh, 1800.0, 130.0, 640.0, xf(), ORIGIN_BASE)
    box(mesh, 1860.0, 190.0, 60.0, xf(z=640.0), ORIGIN_BASE)
    parts["Wall"] = commit(mesh, "SM_NF_Wall", materials["ConcreteLight"])

    mesh = new_mesh()
    cylinder(mesh, 95.0, 980.0, 8, xf(), ORIGIN_BASE)
    cylinder(mesh, 145.0, 70.0, 8, xf(), ORIGIN_BASE)
    cylinder(mesh, 145.0, 70.0, 8, xf(z=910.0), ORIGIN_BASE)
    parts["Pillar"] = commit(mesh, "SM_NF_Pillar", materials["Concrete"])

    mesh = new_mesh()
    box(mesh, 1500.0, 300.0, 30.0, xf(), ORIGIN_BASE)
    box(mesh, 1500.0, 20.0, 110.0, xf(y=-140.0, z=30.0), ORIGIN_BASE)
    box(mesh, 1500.0, 20.0, 110.0, xf(y=140.0, z=30.0), ORIGIN_BASE)
    parts["Catwalk"] = commit(mesh, "SM_NF_Catwalk", materials["SteelWorn"])

    mesh = new_mesh()
    box(mesh, 1100.0, 900.0, 460.0, xf(), ORIGIN_BASE)
    box(mesh, 1180.0, 980.0, 70.0, xf(z=460.0), ORIGIN_BASE)
    box(mesh, 300.0, 240.0, 300.0, xf(x=560.0), ORIGIN_BASE)
    parts["Bunker"] = commit(mesh, "SM_NF_Bunker", materials["Concrete"])

    mesh = new_mesh()
    cylinder(mesh, 380.0, 760.0, 12, xf(), ORIGIN_BASE)
    cone(mesh, 380.0, 250.0, 150.0, 12, xf(z=760.0), ORIGIN_BASE)
    box_ring(mesh, 392.0, 14, 26.0, 24.0, z=380.0)
    parts["Tank"] = commit(mesh, "SM_NF_Tank", materials["SteelWorn"])

    # Stairs are stacked boxes rather than the stairs primitive: same triangle count, and
    # the exact rise and run are set here instead of inferred.
    mesh = new_mesh()
    step_rise, step_run, step_width, step_count = 36.0, 62.0, 400.0, 12
    for index in range(step_count):
        box(mesh, step_width, step_run, step_rise * (index + 1),
            xf(0.0, step_run * index, 0.0), ORIGIN_BASE)
    parts["Stairs"] = commit(mesh, "SM_NF_Stairs", materials["SteelDark"])

    return parts


# --- filament ---------------------------------------------------------------------------------

def build_filament(material):
    """A crossed pair of tapered blades.

    Subdivided along its length so the world position offset bends it into a curve rather
    than shearing it. Four segments is enough to read as a bend and keeps the whole blade
    under fifty triangles.
    """
    mesh = new_mesh()
    for yaw in (0.0, 90.0):
        cone(mesh, 7.0, 1.5, BLADE_HEIGHT, 4, xf(yaw=yaw), ORIGIN_BASE)
    return commit(mesh, "SM_NF_Filament", material)


# --- entry point -------------------------------------------------------------------------------

def build_all(materials):
    banner("meshes")

    result = {
        "terrain": build_terrain(materials["Basalt"]),
        "pylon": build_pylon(materials),
        "drone": build_drone(materials),
        "door": build_door(materials),
        "props": build_props(materials),
        "structures": build_structures(materials),
        "filament": build_filament(materials["Filament"]),
    }

    unreal.log("meshes built")
    return result
