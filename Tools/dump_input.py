"""Print what is actually inside the generated input assets.

The mappings are built from script, and Enhanced Input modifiers are instanced UObjects.
An object created transiently in Python can serialise as null, which would leave keys
mapped but doing the wrong thing - or nothing. This dumps the assets as saved.
"""

import unreal

LOG = unreal.log


def dump(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        LOG("MISSING %s" % path)
        return

    LOG("=" * 70)
    LOG(path)

    if isinstance(asset, unreal.InputMappingContext):
        # The runtime rebuild reads DefaultKeyMappings.Mappings; the top-level Mappings
        # array is deprecated since 5.7 and IGNORED at runtime. Dump what the game reads,
        # and shout if anything is stranded in the dead slot - that exact mistake shipped
        # a build where every physical key was silently unbound.
        deprecated = asset.get_editor_property("mappings")
        if len(deprecated) > 0:
            LOG("  !!! %d mappings in the DEPRECATED slot - the runtime ignores these" % len(deprecated))

        mappings = asset.get_editor_property("default_key_mappings").get_editor_property("mappings")
        LOG("  %d mappings" % len(mappings))
        for mapping in mappings:
            action = mapping.get_editor_property("action")
            key = mapping.get_editor_property("key")
            modifiers = mapping.get_editor_property("modifiers")
            triggers = mapping.get_editor_property("triggers")

            details = []
            for modifier in modifiers:
                if modifier is None:
                    details.append("<NULL MODIFIER>")
                    continue
                name = modifier.get_class().get_name()
                if name == "InputModifierNegate":
                    details.append("Negate(x=%s y=%s z=%s)" % (
                        modifier.get_editor_property("bx"),
                        modifier.get_editor_property("by"),
                        modifier.get_editor_property("bz")))
                elif name == "InputModifierSwizzleAxis":
                    details.append("Swizzle(%s)" % modifier.get_editor_property("order"))
                else:
                    details.append(name)

            LOG("    %-22s %-26s modifiers=[%s] triggers=%d" % (
                unreal.SystemLibrary.get_object_name(action) if action else "<NULL ACTION>",
                key.get_editor_property("key_name") if key else "<NULL KEY>",
                ", ".join(details),
                len(triggers)))

    elif isinstance(asset, unreal.NightfallInputConfig):
        context = asset.get_editor_property("mapping_context")
        LOG("  context: %s" % (unreal.SystemLibrary.get_object_name(context) if context else "<NULL>"))
        for binding in asset.get_editor_property("actions"):
            tag = binding.get_editor_property("input_tag")
            action = binding.get_editor_property("input_action")
            LOG("    %-40s -> %s" % (
                tag.to_string() if hasattr(tag, "to_string") else str(tag),
                unreal.SystemLibrary.get_object_name(action) if action else "<NULL ACTION>"))

    elif isinstance(asset, unreal.InputAction):
        LOG("  value_type: %s" % asset.get_editor_property("value_type"))


dump("/Game/Nightfall/Input/IMC_NF_Default")
dump("/Game/Nightfall/Input/IC_NF_Default")
dump("/Game/Nightfall/Input/IA_NF_Move")
dump("/SurveyScanner/Input/IMC_Scanner")
