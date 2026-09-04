"""Build the Valley level without touching the editor UI.

Run from the command line (see unreal/README.md):
  UnrealEditor-Cmd Hearth.uproject -run=pythonscript -script=Scripts/build_valley_map.py

Creates /Game/Maps/Valley with: a 240 m ground slab, sun + sky + fog, a NavMeshBoundsVolume
covering the play area, a PlayerStart above camp, and HearthGameMode as the level game mode.
Safe to re-run: it overwrites the level.
"""

import unreal

MAP_PATH = "/Game/Maps/Valley"
GROUND_SIZE_M = 240          # the brain's valley is ~100 m across at meters_to_units=10
GROUND_UNITS = GROUND_SIZE_M * 100

log = unreal.log

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# ---- new level -----------------------------------------------------------------
if not level_sub.new_level(MAP_PATH):
    raise RuntimeError(f"could not create level {MAP_PATH}")
log(f"created {MAP_PATH}")

def cosmetic(fn, what):
    """Non-essential tweak: log and continue if the API name differs on this engine version."""
    try:
        fn()
    except Exception as e:  # noqa: BLE001
        unreal.log_warning(f"skipped {what}: {e}")


def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
    a = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a

# ---- ground: a flat cube, top face at z=0 --------------------------------------
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
ground = spawn(unreal.StaticMeshActor, (0, 0, -50), label="Ground")
ground_mesh = ground.get_component_by_class(unreal.StaticMeshComponent)
ground_mesh.set_static_mesh(cube)
ground.set_actor_scale3d(unreal.Vector(GROUND_UNITS / 100.0, GROUND_UNITS / 100.0, 1.0))
ground_mesh.set_mobility(unreal.ComponentMobility.STATIC)
grass = unreal.load_asset("/Engine/EngineMaterials/DefaultMaterial")  # placeholder; swap for landscape later
if grass:
    cosmetic(lambda: ground_mesh.set_material(0, grass), "ground material")

# ---- lighting & sky --------------------------------------------------------------
sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (-40, 30, 0), label="Sun")
sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
cosmetic(lambda: sun_comp.set_editor_property("atmosphere_sun_light", True), "sun atmosphere flag")
cosmetic(lambda: sun_comp.set_intensity(8.0), "sun intensity")
cosmetic(lambda: sun_comp.set_mobility(unreal.ComponentMobility.MOVABLE), "sun mobility")  # day/night later

spawn(unreal.SkyAtmosphere, (0, 0, 0), label="SkyAtmosphere")
sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
cosmetic(lambda: sky_comp.set_editor_property("real_time_capture", True), "sky real-time capture")
cosmetic(lambda: sky_comp.set_mobility(unreal.ComponentMobility.MOVABLE), "sky mobility")
fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0), label="Fog")
cosmetic(lambda: fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.01), "fog density")

# ---- navigation ------------------------------------------------------------------
nav = spawn(unreal.NavMeshBoundsVolume, (0, 0, 0), label="NavBounds")
# default brush is a 200-unit cube; scale to cover the ground and a bit of height
nav.set_actor_scale3d(unreal.Vector(GROUND_UNITS / 200.0, GROUND_UNITS / 200.0, 10.0))

# ---- player start above camp -----------------------------------------------------
spawn(unreal.PlayerStart, (-1200, -1200, 600), (0, 45, 0), label="PlayerStart")

# ---- game mode -------------------------------------------------------------------
world = unreal.EditorLevelLibrary.get_editor_world()
gm = unreal.load_class(None, "/Script/Hearth.HearthGameMode")
if gm is None:
    raise RuntimeError("HearthGameMode class not found; module not compiled?")
world.get_world_settings().set_editor_property("default_game_mode", gm)

# ---- save ------------------------------------------------------------------------
if not level_sub.save_current_level():
    raise RuntimeError("save failed")
log(f"saved {MAP_PATH} with {len(actor_sub.get_all_level_actors())} actors")
