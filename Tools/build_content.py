"""Build the entire Nightfall Relay content set from source.

Run headlessly:

    UnrealEditor-Cmd.exe Nightfall.uproject -run=pythonscript ^
        -script="Tools/build_content.py" -unattended -nosplash

Set NF_STAGES to a comma separated subset to rebuild part of it, for example
NF_STAGES=materials,meshes. Stages run in order and later ones consume the output of
earlier ones, so a subset that skips a dependency reloads it from disk instead.
"""

import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import nf_common  # noqa: E402

STAGE_ORDER = ["materials", "meshes", "data", "level"]


def requested_stages():
    raw = os.environ.get("NF_STAGES", "all").strip().lower()
    if raw in ("", "all"):
        return list(STAGE_ORDER)

    wanted = [s.strip() for s in raw.split(",") if s.strip()]
    unknown = [s for s in wanted if s not in STAGE_ORDER]
    if unknown:
        unreal.log_error("unknown stage(s): %s" % ", ".join(unknown))
    return [s for s in STAGE_ORDER if s in wanted]


def load_materials_from_disk():
    """Reload the material instance palette when the materials stage was skipped."""
    names = {
        "Basalt": "MI_NF_Basalt", "Grit": "MI_NF_Grit",
        "Concrete": "MI_NF_Concrete", "ConcreteLight": "MI_NF_ConcreteLight",
        "Steel": "MI_NF_Steel", "SteelDark": "MI_NF_SteelDark",
        "SteelWorn": "MI_NF_SteelWorn", "PanelAmber": "MI_NF_PanelAmber",
        "PanelCyan": "MI_NF_PanelCyan", "PanelRed": "MI_NF_PanelRed",
        "CellShell": "MI_NF_CellShell", "Filament": "MI_NF_Filament",
    }
    return {key: nf_common.load("materials", name) for key, name in names.items()}


def load_meshes_from_disk():
    """Reload generated meshes when the meshes stage was skipped."""
    import nf_meshes

    def mesh(name):
        return nf_common.load("meshes", name)

    terrain = {}
    for tile_y in range(nf_meshes.TERRAIN_TILES):
        for tile_x in range(nf_meshes.TERRAIN_TILES):
            terrain[(tile_x, tile_y)] = mesh("SM_NF_Terrain_%d_%d" % (tile_x, tile_y))

    return {
        "terrain": terrain,
        "pylon": {
            "Base": mesh("SM_NF_PylonBase"), "Collar": mesh("SM_NF_PylonCollar"),
            "Mast": mesh("SM_NF_PylonMast"), "RingLower": mesh("SM_NF_PylonRingLower"),
            "RingUpper": mesh("SM_NF_PylonRingUpper"), "Core": mesh("SM_NF_PylonCore"),
        },
        "drone": {
            "Hull": mesh("SM_NF_DroneHull"), "YawRing": mesh("SM_NF_DroneYawRing"),
            "PitchArm": mesh("SM_NF_DronePitchArm"), "SensorPod": mesh("SM_NF_DroneSensorPod"),
            "Rotor": mesh("SM_NF_DroneRotor"),
        },
        "door": {
            "Frame": mesh("SM_NF_DoorFrame"), "Leaf": mesh("SM_NF_DoorLeaf"),
            "Wheel": mesh("SM_NF_DoorWheel"), "Panel": mesh("SM_NF_DoorPanel"),
        },
        "props": {
            "PowerCell": mesh("SM_NF_PowerCell"),
            "Rubble0": mesh("SM_NF_Rubble00"), "Rubble1": mesh("SM_NF_Rubble01"),
        },
        "structures": {
            "Platform": mesh("SM_NF_Platform"), "Wall": mesh("SM_NF_Wall"),
            "Pillar": mesh("SM_NF_Pillar"), "Catwalk": mesh("SM_NF_Catwalk"),
            "Bunker": mesh("SM_NF_Bunker"), "Tank": mesh("SM_NF_Tank"),
            "Stairs": mesh("SM_NF_Stairs"),
        },
        "filament": mesh("SM_NF_Filament"),
    }


def main():
    stages = requested_stages()
    nf_common.banner("nightfall content build: %s" % ", ".join(stages))
    nf_common.ensure_directories()
    nf_common.scan_content_roots()

    materials = None
    meshes = None
    data = None

    if "materials" in stages:
        import nf_materials
        materials = nf_materials.build_all()

    if "meshes" in stages:
        import nf_meshes
        materials = materials or load_materials_from_disk()
        meshes = nf_meshes.build_all(materials)

    if "data" in stages:
        import nf_data
        materials = materials or load_materials_from_disk()
        meshes = meshes or load_meshes_from_disk()
        data = nf_data.build_all(materials, meshes)

    if "level" in stages:
        import nf_data
        import nf_level
        materials = materials or load_materials_from_disk()
        meshes = meshes or load_meshes_from_disk()
        data = data or nf_data.load_all()
        nf_level.build(materials, meshes, data)

    nf_common.save_all()
    nf_common.banner("content build complete")


main()
