"""Print the debris scatter graph and the material usage flags as saved.

Both were silent failures: a graph that accumulated a duplicate node pair on every content
rebuild, and materials without the instanced-static-mesh usage flag, which the editor
patches at runtime and a cooked build cannot.
"""

import unreal

LOG = unreal.log


def label(pin):
    try:
        return str(pin.get_editor_property("properties").get_editor_property("label"))
    except Exception:
        return "<?>"


def dump_graph(path):
    graph = unreal.EditorAssetLibrary.load_asset(path)
    if graph is None:
        LOG("MISSING %s" % path)
        return

    LOG("=" * 70)
    LOG(path)
    nodes = list(graph.get_editor_property("nodes"))
    LOG("  %d node(s) besides input and output" % len(nodes))
    for node in nodes:
        settings = node.get_editor_property("settings_interface")
        LOG("    %-24s %s" % (node.get_name(),
                              settings.get_class().get_name() if settings else "?"))

    LOG("  edges:")
    for node in [graph.get_input_node()] + nodes:
        if node is None:
            continue
        for pin in node.get_editor_property("output_pins"):
            for edge in pin.get_editor_property("edges"):
                other = edge.get_editor_property("output_pin")
                if other is None or other.get_editor_property("node") is None:
                    continue
                LOG("    %s.%s -> %s.%s" % (
                    node.get_name(), label(pin),
                    other.get_editor_property("node").get_name(), label(other)))


def dump_material_usage(paths):
    LOG("=" * 70)
    LOG("instanced static mesh usage")
    for path in paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            LOG("  MISSING %s" % path)
            continue
        material = asset
        while isinstance(material, unreal.MaterialInstance):
            material = material.get_editor_property("parent")
        flag = material.get_editor_property("used_with_instanced_static_meshes") if material else "?"
        LOG("  %-46s master=%-16s ism=%s" % (
            path.rsplit("/", 1)[-1],
            material.get_name() if material else "?", flag))


dump_graph("/Game/Nightfall/PCG/PCG_NF_Debris")
dump_material_usage([
    "/Game/Nightfall/Materials/MI_NF_Filament",
    "/Game/Nightfall/Materials/MI_NF_Basalt",
    "/Game/Nightfall/Materials/MI_NF_SteelDark",
    "/Game/Nightfall/Materials/MI_NF_SteelWorn",
])
