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


def spawn(cls, loc=(0, 0, 0), pitch=0.0, yaw=0.0, label=None):
    # unreal.Rotator's positional order is (roll, pitch, yaw); use keywords to avoid pointing the sun up
    a = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
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
sun = spawn(unreal.DirectionalLight, (0, 0, 1000), pitch=-40.0, yaw=30.0, label="Sun")
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

# ---- simple colored materials (generated so no imported assets are needed) --------------
MAT_DIR = "/Game/Materials"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def make_material(name, rgb, roughness=0.9):
    path = f"{MAT_DIR}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = asset_tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    color.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
    return mat

M = {
    "grass": make_material("M_Grass", (0.10, 0.28, 0.08)),
    "bark": make_material("M_Bark", (0.22, 0.13, 0.06)),
    "leaf": make_material("M_Leaf", (0.06, 0.22, 0.07)),
    "water": make_material("M_Water", (0.05, 0.22, 0.45), roughness=0.15),
    "stone": make_material("M_Stone", (0.40, 0.40, 0.38)),
    "sand": make_material("M_Sand", (0.55, 0.48, 0.30)),
    "berry": make_material("M_Berry", (0.35, 0.08, 0.12)),
    "ember": make_material("M_Ember", (0.9, 0.35, 0.05), roughness=0.6),
}
cosmetic(lambda: ground_mesh.set_material(0, M["grass"]), "ground grass material")

# ---- props: the places the brain talks about, so the map isn't an empty plane ---------
SHAPES = {k: unreal.load_asset(f"/Engine/BasicShapes/{v}") for k, v in
          {"cube": "Cube", "cyl": "Cylinder", "cone": "Cone", "sphere": "Sphere", "plane": "Plane"}.items()}
# sim meters * 10 (meters_to_units) -> units. Keep in sync with brain/hearth/world/state.py
LOC = {"camp": (0, 0), "forest": (4500, 1200), "river": (-3800, 2600), "meadow": (1200, -4200), "quarry": (-3000, -3500)}

import random
rng = random.Random(7)
prop_count = 0

def prop(shape, loc, scale, mat, yaw=0.0, label="Prop"):
    global prop_count
    a = spawn(unreal.StaticMeshActor, loc, yaw=yaw, label=label)
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_static_mesh(SHAPES[shape])
    c.set_mobility(unreal.ComponentMobility.STATIC)
    cosmetic(lambda: c.set_material(0, mat), f"material on {label}")
    a.set_actor_scale3d(unreal.Vector(*scale))
    prop_count += 1
    return a

def tree(x, y, size=1.0):
    h = 320 * size
    prop("cyl", (x, y, h / 2), (0.28 * size, 0.28 * size, h / 100), M["bark"], label="Tree_Trunk")
    prop("cone", (x, y, h + 30), (2.6 * size, 2.6 * size, 4.2 * size), M["leaf"], yaw=rng.uniform(0, 360), label="Tree_Canopy")

def ring(cx, cy, r_min, r_max, n):
    for _ in range(n):
        ang = rng.uniform(0, 6.283)
        r = rng.uniform(r_min, r_max)
        import math
        yield cx + math.cos(ang) * r, cy + math.sin(ang) * r

# forest: dense ring of trees around the gathering spot (center kept clear so people can stand)
fx, fy = LOC["forest"]
for x, y in ring(fx, fy, 520, 1700, 45):
    tree(x, y, rng.uniform(0.8, 1.4))

# scattered trees elsewhere for depth (away from the walking lines between places)
for x, y in [(2600, 3200), (3800, -1500), (-1500, 4200), (-4800, -800), (4600, 3600), (-800, -2200), (2200, 1900), (-2600, 900)]:
    tree(x, y, rng.uniform(0.8, 1.3))

# river: a long band of water just past the gathering spot, with a sandy bank
rx, ry = LOC["river"]
prop("cube", (rx - 700, ry + 500, -40), (9.0, 70.0, 0.5), M["water"], yaw=-35, label="River_Water")
prop("cube", (rx - 250, ry + 200, -48), (4.0, 70.0, 0.3), M["sand"], yaw=-35, label="River_Bank")
for x, y in ring(rx, ry, 350, 900, 8):
    prop("sphere", (x, y, 25), (rng.uniform(0.4, 0.9),) * 3, M["stone"], label="River_Rock")

# meadow: lighter grass disc and berry bushes
mx, my = LOC["meadow"]
prop("cyl", (mx, my, -45), (34.0, 34.0, 0.02), M["sand"], label="Meadow_Ground")
for x, y in ring(mx, my, 380, 1300, 22):
    s_ = rng.uniform(0.7, 1.2)
    prop("sphere", (x, y, 45 * s_), (s_, s_, s_ * 0.8), M["leaf"], label="Bush")
    prop("sphere", (x + 20, y - 15, 55 * s_), (s_ * 0.35,) * 3, M["berry"], label="Berries")

# quarry: rock outcrops and boulders
qx, qy = LOC["quarry"]
prop("cyl", (qx, qy, -45), (30.0, 30.0, 0.02), M["stone"], label="Quarry_Ground")
for x, y in ring(qx, qy, 420, 1300, 16):
    s_ = rng.uniform(0.8, 2.2)
    prop("cube", (x, y, 40 * s_), (s_, s_ * rng.uniform(0.6, 1.2), s_ * 0.8), M["stone"], yaw=rng.uniform(0, 360), label="Boulder")
for x, y in ring(qx - 900, qy - 700, 0, 200, 3):
    prop("cube", (x, y, 200), (6.0, 3.0, 4.0), M["stone"], yaw=rng.uniform(0, 360), label="Quarry_Cliff")

# camp: fire pit ring, logs to sit on, a bare dirt patch
cx, cy = LOC["camp"]
prop("cyl", (cx, cy, -45), (14.0, 14.0, 0.02), M["sand"], label="Camp_Dirt")
import math
for i in range(10):
    ang = i * 6.283 / 10
    prop("cube", (cx + math.cos(ang) * 110, cy + math.sin(ang) * 110, 15), (0.35, 0.35, 0.3), M["stone"], yaw=math.degrees(ang), label="FirePit_Stone")
prop("cone", (cx, cy, 10), (0.6, 0.6, 0.9), M["ember"], label="FirePit_Embers")
for ang_deg, dist in ((30, 300), (150, 300), (270, 300)):
    ang = math.radians(ang_deg)
    a = prop("cyl", (cx + math.cos(ang) * dist, cy + math.sin(ang) * dist, 25), (0.5, 0.5, 2.2), M["bark"], label="Log_Seat")
    a.set_actor_rotation(unreal.Rotator(roll=90.0, pitch=0.0, yaw=ang_deg + 90), False)
log(f"placed {prop_count} props")

# ---- navigation ------------------------------------------------------------------
nav = spawn(unreal.NavMeshBoundsVolume, (0, 0, 0), label="NavBounds")
# default brush is a 200-unit cube; scale to cover the ground and a bit of height
nav.set_actor_scale3d(unreal.Vector(GROUND_UNITS / 200.0, GROUND_UNITS / 200.0, 10.0))

# ---- player start above camp -----------------------------------------------------
spawn(unreal.PlayerStart, (-1200, -1200, 600), pitch=-20.0, yaw=45.0, label="PlayerStart")

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
