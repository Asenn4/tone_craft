import math
import os
import sys
import bpy
from PIL import Image

OUT_DIR = r"D:\Self Project\pedal_board\assets\skins\default"
TMP_DIR = r"D:\Self Project\pedal_board\build\blender_tmp"
os.makedirs(OUT_DIR, exist_ok=True)
os.makedirs(TMP_DIR, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.eevee.taa_render_samples = 16
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.compression = 60
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.15

    # Clean dark-neutral ambient world for metallic reflections
    world = bpy.data.worlds.new("StudioWorld")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.08, 0.09, 0.11, 1.0)
        bg.inputs["Strength"].default_value = 0.85

    return scene


def setup_camera_and_lights(scene, ortho_scale=2.55, res_x=96, res_y=96):
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y
    scene.render.resolution_percentage = 100

    cam_data = bpy.data.cameras.new("OrthoCam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho_scale
    cam_obj = bpy.data.objects.new("OrthoCam", cam_data)
    scene.collection.objects.link(cam_obj)
    cam_obj.location = (0.0, 0.0, 6.0)
    cam_obj.rotation_euler = (0.0, 0.0, 0.0)
    scene.camera = cam_obj

    def add_area_light(name, loc, rot_deg, energy, size, color=(1.0, 1.0, 1.0)):
        ld = bpy.data.lights.new(name, type="AREA")
        ld.energy = energy
        ld.size = size
        ld.color = color
        lo = bpy.data.objects.new(name, ld)
        scene.collection.objects.link(lo)
        lo.location = loc
        lo.rotation_euler = tuple(math.radians(a) for a in rot_deg)
        return lo

    # Classic fixed studio lighting (top-left warm key, bottom-right cool rim, overhead soft fill)
    add_area_light("KeyLight", (-3.2, 3.2, 5.0), (28, -28, 0), 320.0, 3.8, (1.0, 0.98, 0.95))
    add_area_light("RimLight", (3.5, -3.0, 3.8), (-32, 34, 0), 165.0, 3.0, (0.82, 0.90, 1.0))
    add_area_light("TopSoft",  (0.0, 0.5, 6.2),  (4, 0, 0),     70.0, 5.5, (1.0, 1.0, 1.0))


def make_pbr_mat(name, base_color, metallic=0.0, roughness=0.35, coat=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*base_color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = coat
    return mat


def add_bevel(obj, width=0.03, segments=3):
    mod = obj.modifiers.new("Bevel", "BEVEL")
    mod.width = width
    mod.segments = segments


def render_knob_filmstrip(filename, frames, style):
    scene = clear_scene()
    setup_camera_and_lights(scene, ortho_scale=2.50, res_x=96, res_y=96)

    root = bpy.data.objects.new("KnobRoot", None)
    scene.collection.objects.link(root)

    black_plastic = make_pbr_mat("BlackPlastic", (0.045, 0.048, 0.055), metallic=0.08, roughness=0.28, coat=0.35)
    dark_gunmetal = make_pbr_mat("Gunmetal",     (0.14, 0.16, 0.19),   metallic=0.88, roughness=0.26)
    silver_chrome = make_pbr_mat("SilverChrome", (0.82, 0.85, 0.89),   metallic=0.96, roughness=0.18)
    gold_brushed  = make_pbr_mat("GoldBrushed",  (0.86, 0.66, 0.24),   metallic=0.95, roughness=0.24)
    cyan_satin    = make_pbr_mat("CyanSatin",    (0.14, 0.62, 0.92),   metallic=0.12, roughness=0.26, coat=0.45)
    white_inlay   = make_pbr_mat("WhiteInlay",   (0.95, 0.96, 0.98),   metallic=0.0,  roughness=0.18)
    red_inlay     = make_pbr_mat("RedInlay",     (0.95, 0.16, 0.20),   metallic=0.0,  roughness=0.18)
    dark_inlay    = make_pbr_mat("DarkInlay",    (0.03, 0.03, 0.035),  metallic=0.0,  roughness=0.30)

    def link_to_root(obj, mat):
        obj.parent = root
        obj.data.materials.append(mat)
        for poly in obj.data.polygons:
            poly.use_smooth = True

    # Soft shadow disc underneath knob (stays fixed on ground, not rotating!)
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=1.06, depth=0.02, location=(0.04, -0.05, 0.0))
    shadow_disc = bpy.context.active_object
    shadow_mat = make_pbr_mat("ShadowRim", (0.015, 0.017, 0.02), metallic=0.0, roughness=0.9)
    shadow_disc.data.materials.append(shadow_mat)

    if style in ("vintage_skirted", "gold_plexi"):
        # Outer tapered skirt
        bpy.ops.mesh.primitive_cone_add(vertices=48, radius1=1.02, radius2=0.84, depth=0.42, location=(0, 0, 0.22))
        skirt = bpy.context.active_object
        add_bevel(skirt, 0.025, 2)
        link_to_root(skirt, black_plastic)

        # Tactile fluted grip ribs around cylinder
        for k in range(12):
            ang = (2.0 * math.pi * k) / 12.0
            rx = math.cos(ang) * 0.80
            ry = math.sin(ang) * 0.80
            bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.075, depth=0.48, location=(rx, ry, 0.44))
            rib = bpy.context.active_object
            link_to_root(rib, black_plastic)

        # Main cylindrical core
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.80, depth=0.50, location=(0, 0, 0.45))
        core = bpy.context.active_object
        add_bevel(core, 0.04, 3)
        link_to_root(core, black_plastic)

        # Metallic brushed top insert cap
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.58, depth=0.08, location=(0, 0, 0.72))
        cap = bpy.context.active_object
        add_bevel(cap, 0.02, 2)
        link_to_root(cap, gold_brushed if style == "gold_plexi" else silver_chrome)

        # Crisp pointer indicator bar pointing +Y (12 o'clock at angle=0)
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.45, 0.75))
        ptr = bpy.context.active_object
        ptr.scale = (0.075, 0.42, 0.06)
        link_to_root(ptr, dark_inlay if style == "gold_plexi" else white_inlay)

    elif style == "chrome_knurled":
        # Heavy knurled chrome/gunmetal cylinder
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.94, depth=0.55, location=(0, 0, 0.30))
        body = bpy.context.active_object
        add_bevel(body, 0.045, 3)
        link_to_root(body, dark_gunmetal)

        for k in range(24):
            ang = (2.0 * math.pi * k) / 24.0
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(math.cos(ang) * 0.93, math.sin(ang) * 0.93, 0.30))
            knurl = bpy.context.active_object
            knurl.scale = (0.045, 0.045, 0.46)
            knurl.rotation_euler = (0, 0, ang)
            link_to_root(knurl, silver_chrome)

        # Beveled dark center face
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.72, depth=0.10, location=(0, 0, 0.60))
        face = bpy.context.active_object
        add_bevel(face, 0.03, 2)
        link_to_root(face, dark_gunmetal)

        # Crimson pointer line
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.42, 0.67))
        ptr = bpy.context.active_object
        ptr.scale = (0.09, 0.46, 0.05)
        link_to_root(ptr, red_inlay)

    else:  # cyan_stomp (classic compact colored boutique pedal knob)
        bpy.ops.mesh.primitive_cone_add(vertices=48, radius1=0.95, radius2=0.80, depth=0.58, location=(0, 0, 0.30))
        body = bpy.context.active_object
        add_bevel(body, 0.04, 3)
        link_to_root(body, cyan_satin)

        for k in range(8):
            ang = (2.0 * math.pi * k) / 8.0
            bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.06, depth=0.52, location=(math.cos(ang) * 0.85, math.sin(ang) * 0.85, 0.30))
            rib = bpy.context.active_object
            link_to_root(rib, cyan_satin)

        # Crisp white indicator notch
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0.44, 0.61))
        ptr = bpy.context.active_object
        ptr.scale = (0.095, 0.44, 0.05)
        link_to_root(ptr, white_inlay)

    strip = Image.new("RGBA", (96, 96 * frames), (0, 0, 0, 0))
    tmp_frame = os.path.join(TMP_DIR, "frame.png")

    # JUCE rotary knob sweeps from -135 deg (min, bottom-left) clockwise to +135 deg (max, bottom-right)
    # In Blender +Z rotation is counter-clockwise, so min = +135 deg, max = -135 deg!
    for i in range(frames):
        t = i / float(frames - 1)
        deg_ccw = 135.0 - t * 270.0
        root.rotation_euler = (0.0, 0.0, math.radians(deg_ccw))
        scene.render.filepath = tmp_frame
        bpy.ops.render.render(write_still=True)
        with Image.open(tmp_frame) as im:
            strip.paste(im.convert("RGBA"), (0, i * 96))

    out_path = os.path.join(OUT_DIR, filename)
    strip.save(out_path, optimize=True)
    print(f"RENDERED: {out_path} ({strip.size})")


def render_footswitch_strip():
    scene = clear_scene()
    setup_camera_and_lights(scene, ortho_scale=2.40, res_x=96, res_y=96)

    chrome = make_pbr_mat("Chrome", (0.86, 0.88, 0.92), metallic=0.98, roughness=0.14)
    washer_mat = make_pbr_mat("Washer", (0.62, 0.66, 0.72), metallic=0.92, roughness=0.28)

    # Outer washer
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=1.02, depth=0.06, location=(0, 0, 0.03))
    w = bpy.context.active_object
    w.data.materials.append(washer_mat)

    # Hex nut (6 vertices!)
    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.84, depth=0.22, location=(0, 0, 0.16))
    nut = bpy.context.active_object
    add_bevel(nut, 0.03, 2)
    nut.data.materials.append(chrome)

    # Threaded collar
    bpy.ops.mesh.primitive_cylinder_add(vertices=36, radius=0.58, depth=0.30, location=(0, 0, 0.30))
    collar = bpy.context.active_object
    collar.data.materials.append(washer_mat)

    # Plunger button
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=0.48, depth=0.45, location=(0, 0, 0.56))
    plunger = bpy.context.active_object
    add_bevel(plunger, 0.08, 4)
    for poly in plunger.data.polygons:
        poly.use_smooth = True
    plunger.data.materials.append(chrome)

    strip = Image.new("RGBA", (96, 192), (0, 0, 0, 0))
    tmp_frame = os.path.join(TMP_DIR, "frame.png")

    for idx, z_pos in enumerate((0.56, 0.38)):
        plunger.location = (0, 0, z_pos)
        scene.render.filepath = tmp_frame
        bpy.ops.render.render(write_still=True)
        with Image.open(tmp_frame) as im:
            strip.paste(im.convert("RGBA"), (0, idx * 96))

    out_path = os.path.join(OUT_DIR, "footswitch_chrome_2.png")
    strip.save(out_path, optimize=True)
    print(f"RENDERED: {out_path}")


def render_toggle_strip():
    scene = clear_scene()
    setup_camera_and_lights(scene, ortho_scale=2.20, res_x=64, res_y=64)

    chrome = make_pbr_mat("Chrome", (0.88, 0.90, 0.94), metallic=0.98, roughness=0.14)
    nut_mat = make_pbr_mat("Nut", (0.65, 0.69, 0.74), metallic=0.92, roughness=0.25)

    bpy.ops.mesh.primitive_cylinder_add(vertices=6, radius=0.52, depth=0.18, location=(0, 0, 0.10))
    nut = bpy.context.active_object
    add_bevel(nut, 0.025, 2)
    nut.data.materials.append(nut_mat)

    # Bat handle pivoted at origin
    pivot = bpy.data.objects.new("BatPivot", None)
    scene.collection.objects.link(pivot)
    pivot.location = (0, 0, 0.16)

    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=0.12, radius2=0.26, depth=0.92, location=(0, 0, 0.46))
    bat = bpy.context.active_object
    bat.parent = pivot
    add_bevel(bat, 0.06, 3)
    for poly in bat.data.polygons:
        poly.use_smooth = True
    bat.data.materials.append(chrome)

    strip = Image.new("RGBA", (64, 128), (0, 0, 0, 0))
    tmp_frame = os.path.join(TMP_DIR, "frame.png")

    for idx, tilt_deg in enumerate((-32.0, 32.0)):  # Frame 0 = UP (ON), Frame 1 = DOWN (OFF)
        pivot.rotation_euler = (math.radians(tilt_deg), 0.0, 0.0)
        scene.render.filepath = tmp_frame
        bpy.ops.render.render(write_still=True)
        with Image.open(tmp_frame) as im:
            strip.paste(im.convert("RGBA"), (0, idx * 64))

    out_path = os.path.join(OUT_DIR, "toggle_chrome_2.png")
    strip.save(out_path, optimize=True)
    print(f"RENDERED: {out_path}")


def render_fader_caps():
    # Studio console fader cap (64x48)
    scene = clear_scene()
    setup_camera_and_lights(scene, ortho_scale=2.20, res_x=64, res_y=48)
    alum = make_pbr_mat("Alum", (0.76, 0.80, 0.86), metallic=0.92, roughness=0.24)
    dark = make_pbr_mat("DarkStripe", (0.04, 0.04, 0.05), metallic=0.1, roughness=0.35)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.25))
    cap = bpy.context.active_object
    cap.scale = (1.65, 1.05, 0.45)
    add_bevel(cap, 0.08, 3)
    cap.data.materials.append(alum)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 0.48))
    stripe = bpy.context.active_object
    stripe.scale = (1.58, 0.12, 0.06)
    stripe.data.materials.append(dark)

    out_console = os.path.join(OUT_DIR, "fader_console_cap.png")
    scene.render.filepath = out_console
    bpy.ops.render.render(write_still=True)
    print(f"RENDERED: {out_console}")


if __name__ == "__main__":
    render_knob_filmstrip("knob_vintage_skirted_64.png", 64, "vintage_skirted")
    render_knob_filmstrip("knob_gold_plexi_64.png", 64, "gold_plexi")
    render_knob_filmstrip("knob_chrome_knurled_64.png", 64, "chrome_knurled")
    render_knob_filmstrip("knob_cyan_stomp_64.png", 64, "cyan_stomp")
    render_footswitch_strip()
    render_toggle_strip()
    render_fader_caps()
    print("ALL BLENDER FILMSTRIPS COMPLETE!")
