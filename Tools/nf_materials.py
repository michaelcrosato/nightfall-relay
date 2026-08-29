"""Material graphs, built node by node.

Two masters carry the whole project.

M_NF_Surface is a flat parameterised PBR material with an emissive term. There is no
texturing anywhere in this project and that is on purpose: at this triangle count, with
Lumen bouncing light around and MegaLights putting a shadow on every fixture, surface
detail comes almost entirely from the lighting. Flat albedo also keeps the frame cheap
where it needs to be so it can be expensive where it counts.

M_NF_Filament adds world position offset. Filaments bend in the vertex shader from three
material parameters the field actor writes each frame, plus each instance's own random, so
thousands of them cost one draw and no CPU work.
"""

import unreal

from nf_common import banner, create, linear, save

MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty


# --- graph helpers --------------------------------------------------------------------

def node(material, expression_class, x, y):
    return MEL.create_material_expression(material, expression_class, x, y)


def scalar_param(material, name, value, x, y, group="", sort=0):
    expression = node(material, unreal.MaterialExpressionScalarParameter, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", value)
    if group:
        expression.set_editor_property("group", group)
    expression.set_editor_property("sort_priority", sort)
    return expression


def vector_param(material, name, color, x, y, group="", sort=0):
    expression = node(material, unreal.MaterialExpressionVectorParameter, x, y)
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", color)
    if group:
        expression.set_editor_property("group", group)
    expression.set_editor_property("sort_priority", sort)
    return expression


def constant(material, value, x, y):
    expression = node(material, unreal.MaterialExpressionConstant, x, y)
    expression.set_editor_property("r", value)
    return expression


def multiply(material, a, b, x, y, a_out="", b_out=""):
    expression = node(material, unreal.MaterialExpressionMultiply, x, y)
    MEL.connect_material_expressions(a, a_out, expression, "A")
    MEL.connect_material_expressions(b, b_out, expression, "B")
    return expression


def add(material, a, b, x, y, a_out="", b_out=""):
    expression = node(material, unreal.MaterialExpressionAdd, x, y)
    MEL.connect_material_expressions(a, a_out, expression, "A")
    MEL.connect_material_expressions(b, b_out, expression, "B")
    return expression


def lerp(material, a, b, alpha, x, y, alpha_out=""):
    expression = node(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, "", expression, "A")
    MEL.connect_material_expressions(b, "", expression, "B")
    MEL.connect_material_expressions(alpha, alpha_out, expression, "Alpha")
    return expression


def component_mask(material, source, r, g, b, a, x, y, source_out=""):
    expression = node(material, unreal.MaterialExpressionComponentMask, x, y)
    expression.set_editor_property("r", r)
    expression.set_editor_property("g", g)
    expression.set_editor_property("b", b)
    expression.set_editor_property("a", a)
    MEL.connect_material_expressions(source, source_out, expression, "")
    return expression


def divide(material, a, b, x, y, a_out="", b_out=""):
    expression = node(material, unreal.MaterialExpressionDivide, x, y)
    MEL.connect_material_expressions(a, a_out, expression, "A")
    MEL.connect_material_expressions(b, b_out, expression, "B")
    return expression


def clamp01(material, source, x, y, source_out=""):
    expression = node(material, unreal.MaterialExpressionClamp, x, y)
    MEL.connect_material_expressions(source, source_out, expression, "")
    return expression


def append(material, a, b, x, y, a_out="", b_out=""):
    expression = node(material, unreal.MaterialExpressionAppendVector, x, y)
    MEL.connect_material_expressions(a, a_out, expression, "A")
    MEL.connect_material_expressions(b, b_out, expression, "B")
    return expression


# --- masters --------------------------------------------------------------------------

# --- masters ----------------------------------------------------------------------------

def enable_instanced_usage(material):
    """Allow the material to be drawn by instanced static mesh components.

    Without this the editor quietly recompiles the shader the first time an ISM asks for
    it - "Had to pass SMU back to game thread" in the log - and everything looks right.
    A cooked build cannot recompile, so it silently substitutes the default material and
    the filament field loses both its colour and its world position offset. The flag has
    to be authored, not discovered at runtime.
    """
    material.set_editor_property("used_with_instanced_static_meshes", True)


def build_surface_master():
    """Flat PBR plus a three-parameter emissive term.

    EmissiveColor, EmissiveLevel and EmissiveIntensity are the exact names the machine and
    prop classes drive on their dynamic material instances, so any instance of this master
    animates correctly the moment it is placed on a part.
    """
    material = create("materials", "M_NF_Surface", unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        return None

    base_color = vector_param(material, "BaseColor", linear(0.055, 0.060, 0.068), -820, -260, "1 Surface", 0)
    roughness = scalar_param(material, "Roughness", 0.62, -820, -80, "1 Surface", 1)
    metallic = scalar_param(material, "Metallic", 0.0, -820, 20, "1 Surface", 2)
    specular = scalar_param(material, "Specular", 0.42, -820, 120, "1 Surface", 3)

    emissive_color = vector_param(material, "EmissiveColor", linear(1.0, 0.62, 0.18), -820, 300, "2 Emissive", 0)
    emissive_level = scalar_param(material, "EmissiveLevel", 0.0, -820, 460, "2 Emissive", 1)
    emissive_intensity = scalar_param(material, "EmissiveIntensity", 18.0, -820, 560, "2 Emissive", 2)

    # Colour times level times intensity: level is the animated 0..1 the game drives, and
    # intensity is the authored brightness of that surface.
    scaled_level = multiply(material, emissive_level, emissive_intensity, -520, 500)
    emissive = multiply(material, emissive_color, scaled_level, -300, 380)

    MEL.connect_material_property(base_color, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(roughness, "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(metallic, "", MP.MP_METALLIC)
    MEL.connect_material_property(specular, "", MP.MP_SPECULAR)
    MEL.connect_material_property(emissive, "", MP.MP_EMISSIVE_COLOR)

    enable_instanced_usage(material)
    MEL.recompile_material(material)
    save(material)
    return material


def build_filament_master():
    """Opaque material whose vertices bend under a wind vector.

    The height mask comes from vertex colour red, written when the filament mesh is built,
    rather than from UVs or local position - it is the one channel guaranteed to mean the
    same thing on every vertex of a generated mesh.
    """
    material = create("materials", "M_NF_Filament", unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        return None

    # Bending is a displacement, so tell the renderer how far it can reach. Leaving this at
    # the default inflates every instance's bounds and costs culling accuracy.
    material.set_editor_property("max_world_position_offset_displacement", 90.0)

    base_color = vector_param(material, "BaseColor", linear(0.030, 0.036, 0.040), -1180, -400, "1 Surface", 0)
    tip_color = vector_param(material, "TipColor", linear(0.10, 0.55, 0.72), -1180, -250, "1 Surface", 1)
    roughness = scalar_param(material, "Roughness", 0.74, -1180, -100, "1 Surface", 2)
    night_glow = scalar_param(material, "NightGlow", 0.0, -1180, 0, "1 Surface", 3)
    glow_intensity = scalar_param(material, "GlowIntensity", 2.6, -1180, 100, "1 Surface", 4)

    wind_vector = vector_param(material, "WindVector", linear(1.0, 0.0, 0.0), -1180, 300, "2 Wind", 0)
    gust_strength = scalar_param(material, "GustStrength", 0.4, -1180, 450, "2 Wind", 1)
    sway_amplitude = scalar_param(material, "SwayAmplitude", 46.0, -1180, 550, "2 Wind", 2)
    sway_frequency = scalar_param(material, "SwayFrequency", 1.35, -1180, 650, "2 Wind", 3)

    # --- height mask: 0 at the root, 1 at the tip ------------------------------------
    # Read straight off the mesh's own local Z rather than baking a mask into vertex
    # colours or relying on how a generated primitive happened to lay out its UVs. For an
    # instanced mesh this is the instance's own space, which is exactly what a blade wants.
    blade_height = scalar_param(material, "BladeHeight", 180.0, -1180, 750, "2 Wind", 4)
    local_position = node(material, unreal.MaterialExpressionLocalPosition, -1180, -560)
    local_z = component_mask(material, local_position, False, False, True, False, -1000, -560, "")
    height_raw = divide(material, local_z, blade_height, -860, -560)
    height = clamp01(material, height_raw, -720, -560)

    # Squaring it keeps the base planted and puts all the motion in the top half.
    height_sq = multiply(material, height, height, -580, -620)

    # --- per-instance phase, so no two filaments swing together -----------------------
    per_instance = node(material, unreal.MaterialExpressionPerInstanceRandom, -980, 760)
    tau = constant(material, 6.2831853, -980, 860)
    phase = multiply(material, per_instance, tau, -800, 800)

    time_node = node(material, unreal.MaterialExpressionTime, -980, 640)
    time_scaled = multiply(material, time_node, sway_frequency, -800, 660)
    wave_input = add(material, time_scaled, phase, -620, 700)
    wave = node(material, unreal.MaterialExpressionSine, -460, 700)
    MEL.connect_material_expressions(wave_input, "", wave, "")

    # Bias the sine so a filament never fully unbends: wind is a steady push with a
    # flutter on top, not an oscillation about rest.
    wave_half = multiply(material, wave, constant(material, 0.35, -460, 840), -300, 760)
    wave_biased = add(material, wave_half, constant(material, 0.65, -300, 880), -160, 800)

    # --- combine into a horizontal offset ----------------------------------------------
    wind_xy = component_mask(material, wind_vector, True, True, False, False, -980, 300)
    push = multiply(material, wind_xy, gust_strength, -800, 320)
    push = multiply(material, push, sway_amplitude, -640, 340)
    push = multiply(material, push, height_sq, -480, 360)
    push = multiply(material, push, wave_biased, -320, 400)

    offset = append(material, push, constant(material, 0.0, -320, 520), -160, 440)

    # --- surface ------------------------------------------------------------------------
    albedo = lerp(material, base_color, tip_color, height, -560, -340)
    glow_scaled = multiply(material, night_glow, glow_intensity, -800, 60)
    glow_masked = multiply(material, glow_scaled, height_sq, -620, 80)
    emissive = multiply(material, tip_color, glow_masked, -440, 20)

    MEL.connect_material_property(albedo, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(roughness, "", MP.MP_ROUGHNESS)
    MEL.connect_material_property(emissive, "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(offset, "", MP.MP_WORLD_POSITION_OFFSET)

    enable_instanced_usage(material)
    MEL.recompile_material(material)
    save(material)
    return material


# --- instances --------------------------------------------------------------------------

def make_instance(name, parent, scalars=None, vectors=None):
    instance = create("materials", name, unreal.MaterialInstanceConstant,
                      unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        return None

    instance.set_editor_property("parent", parent)

    for key, value in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(instance, key, value)
    for key, value in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(instance, key, value)

    MEL.update_material_instance(instance)
    save(instance)
    return instance


def build_all():
    """Build both masters and the palette of instances the world is dressed with."""
    banner("materials")

    surface = build_surface_master()
    filament = build_filament_master()
    if surface is None or filament is None:
        return {}

    # Cold Iron and Sodium: everything structural is a desaturated blue-grey with a
    # deliberately narrow albedo range, so all the colour in a frame arrives as light.
    instances = {
        "Basalt": make_instance("MI_NF_Basalt", surface,
                                {"Roughness": 0.86, "Metallic": 0.0, "Specular": 0.30},
                                {"BaseColor": linear(0.0345, 0.0370, 0.0420)}),
        "Grit": make_instance("MI_NF_Grit", surface,
                              {"Roughness": 0.92, "Metallic": 0.0, "Specular": 0.26},
                              {"BaseColor": linear(0.0430, 0.0445, 0.0475)}),
        "Concrete": make_instance("MI_NF_Concrete", surface,
                                  {"Roughness": 0.78, "Metallic": 0.0, "Specular": 0.38},
                                  {"BaseColor": linear(0.0480, 0.0505, 0.0545)}),
        "ConcreteLight": make_instance("MI_NF_ConcreteLight", surface,
                                       {"Roughness": 0.72, "Metallic": 0.0, "Specular": 0.40},
                                       {"BaseColor": linear(0.0720, 0.0755, 0.0810)}),
        "Steel": make_instance("MI_NF_Steel", surface,
                               {"Roughness": 0.42, "Metallic": 1.0, "Specular": 0.55},
                               {"BaseColor": linear(0.0620, 0.0670, 0.0740)}),
        "SteelDark": make_instance("MI_NF_SteelDark", surface,
                                   {"Roughness": 0.34, "Metallic": 1.0, "Specular": 0.60},
                                   {"BaseColor": linear(0.0330, 0.0360, 0.0410)}),
        "SteelWorn": make_instance("MI_NF_SteelWorn", surface,
                                   {"Roughness": 0.58, "Metallic": 1.0, "Specular": 0.48},
                                   {"BaseColor": linear(0.0850, 0.0790, 0.0700)}),
        # Emissive instances start dark. The game drives EmissiveLevel; leaving it at zero
        # means an unpowered pylon really is unlit rather than merely dim.
        "PanelAmber": make_instance("MI_NF_PanelAmber", surface,
                                    {"Roughness": 0.30, "Metallic": 0.0, "Specular": 0.50,
                                     "EmissiveLevel": 0.0, "EmissiveIntensity": 11.0},
                                    {"BaseColor": linear(0.0300, 0.0250, 0.0180),
                                     "EmissiveColor": linear(1.0, 0.62, 0.18)}),
        "PanelCyan": make_instance("MI_NF_PanelCyan", surface,
                                   {"Roughness": 0.28, "Metallic": 0.0, "Specular": 0.50,
                                    "EmissiveLevel": 0.0, "EmissiveIntensity": 9.0},
                                   {"BaseColor": linear(0.0180, 0.0280, 0.0330),
                                    "EmissiveColor": linear(0.16, 0.86, 1.0)}),
        "PanelRed": make_instance("MI_NF_PanelRed", surface,
                                  {"Roughness": 0.30, "Metallic": 0.0, "Specular": 0.50,
                                   "EmissiveLevel": 0.0, "EmissiveIntensity": 10.0},
                                  {"BaseColor": linear(0.0330, 0.0160, 0.0160),
                                   "EmissiveColor": linear(1.0, 0.12, 0.14)}),
        "CellShell": make_instance("MI_NF_CellShell", surface,
                                   {"Roughness": 0.36, "Metallic": 0.4, "Specular": 0.55,
                                    "EmissiveLevel": 1.0, "EmissiveIntensity": 5.0},
                                   {"BaseColor": linear(0.0400, 0.0470, 0.0520),
                                    "EmissiveColor": linear(0.16, 0.86, 1.0)}),
        "Filament": make_instance("MI_NF_Filament", filament,
                                  {"Roughness": 0.74, "GlowIntensity": 3.1,
                                   "SwayAmplitude": 46.0, "SwayFrequency": 1.35,
                                   "BladeHeight": 180.0},
                                  {"BaseColor": linear(0.0230, 0.0270, 0.0300),
                                   "TipColor": linear(0.09, 0.48, 0.62)}),
    }

    made = len([v for v in instances.values() if v is not None])
    unreal.log("built 2 masters and %d instances" % made)
    return instances
