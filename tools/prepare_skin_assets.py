"""
Prepare and render photorealistic PNG hardware plates for PedalBoard v0.5.0.
Combines the generated studio photographs (wood deck, pearl-white stompbox, 4x12 studio cab)
with 2x-supersampled NumPy/Pillow 3D PBR renders so every stompbox, amp head, cabinet,
toggle switch, and sidebar/chain thumbnail has genuine studio depth and lighting.
"""

import math
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

ART_DIR = r"C:\Users\LENOVO\.gemini\antigravity\brain\f6771e88-42fc-4294-a95c-9a3b09f1fae1"
OUT_DIR = r"d:\Self Project\pedal_board\assets\skins\default"
os.makedirs(OUT_DIR, exist_ok=True)


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / np.maximum(e1 - e0, 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise_2d(h, w, cell_size, seed=42):
    rng = np.random.default_rng(seed)
    gh = max(2, h // cell_size + 2)
    gw = max(2, w // cell_size + 2)
    grid = rng.random((gh, gw)).astype(np.float32)
    ys = np.arange(h, dtype=np.float32) / cell_size
    xs = np.arange(w, dtype=np.float32) / cell_size
    y0 = np.floor(ys).astype(np.int32)
    x0 = np.floor(xs).astype(np.int32)
    fy = ys - y0
    fx = xs - x0
    fy = fy * fy * (3.0 - 2.0 * fy)
    fx = fx * fx * (3.0 - 2.0 * fx)
    v00 = grid[y0[:, None], x0[None, :]]
    v01 = grid[y0[:, None], x0[None, :] + 1]
    v10 = grid[y0[:, None] + 1, x0[None, :]]
    v11 = grid[y0[:, None] + 1, x0[None, :] + 1]
    top = v00 * (1.0 - fx[None, :]) + v01 * fx[None, :]
    bot = v10 * (1.0 - fx[None, :]) + v11 * fx[None, :]
    return top * (1.0 - fy[:, None]) + bot * fy[:, None]


def fbm_2d(h, w, base_cell=32, octaves=4, seed=42):
    out = np.zeros((h, w), dtype=np.float32)
    amp = 0.55
    cell = base_cell
    norm = 0.0
    for i in range(octaves):
        out += amp * value_noise_2d(h, w, max(2, cell), seed=seed + i * 31)
        norm += amp
        amp *= 0.5
        cell //= 2
    return out / max(norm, 1e-6)


def rounded_rect_sdf(xx, yy, cx, cy, hw, hh, r):
    dx = np.abs(xx - cx) - (hw - r)
    dy = np.abs(yy - cy) - (hh - r)
    qx = np.maximum(dx, 0.0)
    qy = np.maximum(dy, 0.0)
    return np.sqrt(qx * qx + qy * qy) + np.minimum(np.maximum(dx, dy), 0.0) - r


def shade_from_height(height_map, mask, albedo_rgb, metallic=0.15, roughness=0.35, light_dir=(-0.42, -0.55, 0.72), scale=18.0):
    gy, gx = np.gradient(height_map)
    nx = -gx * scale
    ny = -gy * scale
    nz = np.ones_like(height_map)
    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz + 1e-8)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    lx, ly, lz = light_dir
    ll = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ll, ly / ll, lz / ll

    ndotl = np.clip(nx * lx + ny * ly + nz * lz, 0.0, 1.0)
    # Secondary warm fill light from lower-right
    l2x, l2y, l2z = 0.55, 0.35, 0.75
    l2l = math.sqrt(l2x * l2x + l2y * l2y + l2z * l2z)
    ndotl2 = np.clip(nx * (l2x / l2l) + ny * (l2y / l2l) + nz * (l2z / l2l), 0.0, 1.0)

    # Half-vector for Blinn-Phong studio specular
    hx, hy, hz = lx, ly, lz + 1.0
    hl = math.sqrt(hx * hx + hy * hy + hz * hz)
    ndoth = np.clip(nx * (hx / hl) + ny * (hy / hl) + nz * (hz / hl), 0.0, 1.0)
    shininess = max(4.0, (1.0 - roughness) ** 2 * 140.0 + 8.0)
    spec = (ndoth ** shininess) * (0.28 + 0.72 * (1.0 - roughness))

    # Subtle Fresnel rim highlight
    rim = (1.0 - np.clip(nz, 0.0, 1.0)) ** 2.2 * (0.22 + 0.35 * metallic)

    ambient = 0.24
    diff = ambient + 0.78 * ndotl + 0.16 * ndotl2
    spec_col = albedo_rgb * metallic + np.array([1.0, 0.99, 0.97], dtype=np.float32) * (1.0 - metallic)
    rgb = albedo_rgb * diff[..., None] * (1.0 - 0.35 * metallic) + spec_col * (spec + rim)[..., None] * (0.45 + 0.55 * metallic)
    rgb = np.clip(rgb, 0.0, 1.0)
    return rgb


def save_png(img: Image.Image, filename: str, optimize: bool = True):
    path = os.path.join(OUT_DIR, filename)
    img.save(path, format="PNG", optimize=optimize)
    sz = os.path.getsize(path)
    print(f"  [OK] {filename:28s} {img.size[0]:4d}x{img.size[1]:4d} ({img.mode:4s}) -> {sz/1024:6.1f} KB")


# ------------------------------------------------------------------------------
# 1. Weathered Wood Pedalboard Deck (from generated studio photo)
# ------------------------------------------------------------------------------
def prepare_wood_deck():
    src = Image.open(os.path.join(ART_DIR, "pedalboard_wood_1795439512037.jpg")).convert("RGB")
    # Resize cleanly to 1280x640 (crop slight vertical aspect if needed)
    w, h = src.size
    target_w, target_h = 1280, 640
    # Crop centered 2:1 band
    crop_h = int(w * (target_h / target_w))
    if crop_h < h:
        y0 = (h - crop_h) // 2
        src = src.crop((0, y0, w, y0 + crop_h))
    wood = src.resize((target_w, target_h), Image.Resampling.LANCZOS)

    # Rich warm dark stage grading + top/bottom subtle vignette like Screenshot 1
    arr = np.asarray(wood, dtype=np.float32) / 255.0
    yy, xx = np.mgrid[0:target_h, 0:target_w].astype(np.float32)
    nx = (xx / target_w) * 2.0 - 1.0
    ny = (yy / target_h) * 2.0 - 1.0
    # Warm center stage spotlight, darker outer corners & subtle bottom floor shadow
    vig = 1.0 - 0.28 * (nx * nx * 0.7 + ny * ny * 0.5)
    top_shade = 0.88 + 0.12 * smoothstep(0.0, 0.18, yy / target_h)
    bot_shade = 0.80 + 0.20 * (1.0 - smoothstep(0.84, 1.0, yy / target_h))
    arr = arr * (vig * top_shade * bot_shade)[..., None]
    # Warm mahogany/walnut tint
    arr[..., 0] *= 1.04
    arr[..., 1] *= 0.98
    arr[..., 2] *= 0.88
    arr = np.clip(arr * 255.0, 0, 255).astype(np.uint8)
    out = Image.fromarray(arr, mode="RGB")
    save_png(out, "pedalboard_wood.png")


# ------------------------------------------------------------------------------
# 2. Classic Die-Cast Stompbox Plates (White + Weathered Yellow + Weathered Orange)
#    Derived from genuine studio photograph stomp_comp_white_1795439514532.jpg
# ------------------------------------------------------------------------------
def prepare_diecast_stompboxes():
    src = Image.open(os.path.join(ART_DIR, "stomp_comp_white_1795439514532.jpg")).convert("RGB")
    arr = np.asarray(src, dtype=np.float32) / 255.0
    luma = 0.299 * arr[..., 0] + 0.587 * arr[..., 1] + 0.114 * arr[..., 2]

    # Clean matte from solid black studio backdrop
    # Find bounding box of pedal silhouette
    fg_raw = luma > 0.045
    ys = np.where(fg_raw.any(axis=1))[0]
    xs = np.where(fg_raw.any(axis=0))[0]
    y0, y1 = int(ys[0]), int(ys[-1])
    x0, x1 = int(xs[0]), int(xs[-1])

    # Add modest padding for studio shadow
    pad_x = max(12, int((x1 - x0) * 0.06))
    pad_y = max(12, int((y1 - y0) * 0.04))
    x0 = max(0, x0 - pad_x)
    x1 = min(arr.shape[1] - 1, x1 + pad_x)
    y0 = max(0, y0 - pad_y)
    y1 = min(arr.shape[0] - 1, y1 + pad_y)

    crop_rgb = src.crop((x0, y0, x1 + 1, y1 + 1))
    target_w, target_h = 300, 480
    base_rgb_img = crop_rgb.resize((target_w, target_h), Image.Resampling.LANCZOS)
    base_rgb = np.asarray(base_rgb_img, dtype=np.float32) / 255.0
    base_luma = 0.299 * base_rgb[..., 0] + 0.587 * base_rgb[..., 1] + 0.114 * base_rgb[..., 2]

    # Flood-fill outside black region to build exact exterior mask (never knocks out dark rubber pad or recessed bay!)
    H, W = base_luma.shape
    outside = np.zeros((H, W), dtype=bool)
    bg_cand = base_luma < 0.055
    stack = []
    for x in range(W):
        if bg_cand[0, x]:
            outside[0, x] = True
            stack.append((0, x))
        if bg_cand[H - 1, x]:
            outside[H - 1, x] = True
            stack.append((H - 1, x))
    for y in range(H):
        if bg_cand[y, 0]:
            outside[y, 0] = True
            stack.append((y, 0))
        if bg_cand[y, W - 1]:
            outside[y, W - 1] = True
            stack.append((y, W - 1))

    while stack:
        cy, cx = stack.pop()
        for ny, nx in ((cy - 1, cx), (cy + 1, cx), (cy, cx - 1), (cy, cx + 1)):
            if 0 <= ny < H and 0 <= nx < W and not outside[ny, nx] and bg_cand[ny, nx]:
                outside[ny, nx] = True
                stack.append((ny, nx))

    inside = (~outside).astype(np.float32)
    # Soft anti-aliased edge
    alpha_img = Image.fromarray((inside * 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=0.8))
    alpha = smoothstep(0.18, 0.82, np.asarray(alpha_img, dtype=np.float32) / 255.0)

    # Measure normalized vertical landmarks of the pedal body for exact C++ layout alignment
    body_ys = np.where(alpha > 0.5)[0]
    body_xs = np.where(alpha > 0.5)[1]
    print(f"  [Info] Die-cast stomp silhouette: x={body_xs[0]/W:.3f}..{body_xs[-1]/W:.3f}, y={body_ys[0]/H:.3f}..{body_ys[-1]/H:.3f}")

    # Soft studio contact drop shadow on wood floor
    shadow_src = Image.fromarray((alpha * 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=7.0))
    shadow = np.asarray(shadow_src, dtype=np.float32) / 255.0
    shadow_shifted = np.zeros_like(shadow)
    dy_sh, dx_sh = 8, 2
    shadow_shifted[dy_sh:, dx_sh:] = shadow[:-dy_sh, :-dx_sh]

    def composite_with_shadow(rgb_arr, a_arr):
        # Premultiply-aware over composition: foreground over dark translucent shadow
        sh_a = shadow_shifted * 0.62 * (1.0 - a_arr)
        out_a = a_arr + sh_a
        out_rgb = np.where(out_a[..., None] > 1e-4, (rgb_arr * a_arr[..., None]) / np.maximum(out_a[..., None], 1e-4), 0.0)
        rgba = np.dstack([
            np.clip(out_rgb * 255.0, 0, 255).astype(np.uint8),
            np.clip(out_a * 255.0, 0, 255).astype(np.uint8),
        ])
        return Image.fromarray(rgba, mode="RGBA")

    # 2A. Pearl White Stompbox (Compressor)
    # Slight clean studio brightness lift on white body while preserving deep dark cavity & rubber ribs
    white_rgb = np.clip(base_rgb * 1.04, 0.0, 1.0)
    save_png(composite_with_shadow(white_rgb, alpha), "stomp_comp_white.png")

    # Build painted-enclosure mask (excludes dark charcoal upper knob cavity & black ribbed rubber pad!)
    # Notice in the photo: painted pearl-white shell has luma > 0.48 everywhere, while upper cavity is ~0.25-0.36 and rubber pad is ~0.04-0.20!
    paint_mask = smoothstep(0.44, 0.58, base_luma) * alpha

    # Compute edge proximity for authentic road-worn paint chips along lips, bevels, and bottom rim
    eroded = np.asarray(
        Image.fromarray((paint_mask * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(7)),
        dtype=np.float32
    ) / 255.0
    edge_band = np.clip(paint_mask - eroded, 0.0, 1.0)
    # Also detect horizontal step bevels (where luminance gradient is steep)
    gy_l, gx_l = np.gradient(base_luma)
    bevel_edge = smoothstep(0.025, 0.08, np.sqrt(gx_l * gx_l + gy_l * gy_l)) * paint_mask
    wear_zone = np.clip(edge_band * 1.25 + bevel_edge * 0.75, 0.0, 1.0)

    def make_weathered_variant(target_hex, chip_seed, filename, dark_cavity_tint=(0.82, 0.82, 0.84)):
        tr = ((target_hex >> 16) & 255) / 255.0
        tg = ((target_hex >> 8) & 255) / 255.0
        tb = (target_hex & 255) / 255.0
        tint = np.array([tr, tg, tb], dtype=np.float32)

        # Normalize luminance around pearl white (~0.83) so authentic studio speculars & bevel shading transfer 1:1
        rel_luma = base_luma / 0.83
        # Specular lift where photo is brightest (> 0.90)
        spec_lift = smoothstep(0.88, 0.99, base_luma) * 0.28
        painted = np.clip(tint[None, None, :] * (rel_luma[..., None] ** 1.15) + spec_lift[..., None], 0.0, 1.0)

        # Subtle painted surface micro-grit & vignette
        grit = (fbm_2d(H, W, base_cell=20, octaves=3, seed=chip_seed + 7) - 0.5) * 0.05
        painted = np.clip(painted + grit[..., None], 0.0, 1.0)

        # Subtle, natural road-worn paint chips along corners & bevel lips (like Screenshot 1!)
        n1 = fbm_2d(H, W, base_cell=18, octaves=4, seed=chip_seed)
        n2 = fbm_2d(H, W, base_cell=6, octaves=3, seed=chip_seed + 99)
        chip_field = wear_zone * (0.60 * n1 + 0.55 * n2)
        chip_mask = smoothstep(0.55, 0.70, chip_field) * paint_mask
        metal_glint = smoothstep(0.72, 0.84, chip_field) * chip_mask

        primer_col = np.array([0.15, 0.13, 0.11], dtype=np.float32)
        bare_metal = np.array([0.52, 0.53, 0.56], dtype=np.float32)
        chipped = painted * (1.0 - chip_mask[..., None]) + primer_col * chip_mask[..., None]
        chipped = chipped * (1.0 - metal_glint[..., None] * 0.7) + bare_metal * (metal_glint[..., None] * 0.7)

        # Slightly darken charcoal cavity so yellow/orange contrast pops like Screenshot 1
        cavity_rgb = base_rgb * np.array(dark_cavity_tint, dtype=np.float32)[None, None, :]
        final_rgb = cavity_rgb * (1.0 - paint_mask[..., None]) + chipped * paint_mask[..., None]
        save_png(composite_with_shadow(final_rgb, alpha), filename)

    # 2B. Weathered Vivid Yellow Overdrive (like Screenshot 1 Pedal #1)
    make_weathered_variant(0xF6BE18, chip_seed=101, filename="stomp_od_yellow.png", dark_cavity_tint=(0.58, 0.58, 0.60))

    # 2C. Weathered Vivid Tangerine Orange Noise Gate (like Screenshot 1 Pedal #2)
    make_weathered_variant(0xF25C13, chip_seed=207, filename="stomp_gate_orange.png", dark_cavity_tint=(0.58, 0.58, 0.60))


# ------------------------------------------------------------------------------
# 3. Glossy Emerald Green Boutique Stompboxes (EQ-7 + Chorus) — 2x Supersampled PBR
# ------------------------------------------------------------------------------
def render_boutique_green_pedal(out_w, out_h, filename, color_top=(0.06, 0.62, 0.14), color_bot=(0.02, 0.34, 0.08),
                                add_recess_window=False):
    S = 2
    W, H = out_w * S, out_h * S
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    cx, cy = W * 0.5, H * 0.5
    hw, hh = W * 0.43, H * 0.44
    r = W * 0.055

    sdf = rounded_rect_sdf(xx, yy, cx, cy, hw, hh, r)
    alpha = 1.0 - smoothstep(-1.5, 1.5, sdf)

    # 3D beveled die-cast metal dome heightmap
    bevel_w = W * 0.042
    edge_t = smoothstep(0.0, bevel_w, -sdf)
    height = np.sqrt(np.clip(edge_t * (2.0 - edge_t), 0.0, 1.0)) * 0.90
    # Slight convex metallic crown
    dxn = (xx - cx) / hw
    dyn = (yy - cy) / hh
    height += (1.0 - 0.12 * (dxn * dxn + dyn * dyn)) * 0.10 * alpha

    # Subtle powder-coat orange-peel micro texture
    coat = (fbm_2d(H, W, base_cell=10, octaves=3, seed=33) - 0.5) * 0.012
    height += coat * alpha

    # Optional subtle dark recessed tray for Graphic EQ faders
    recess_mask = np.zeros((H, W), dtype=np.float32)
    if add_recess_window:
        rsdf = rounded_rect_sdf(xx, yy, cx, H * 0.39, hw * 0.84, hh * 0.34, W * 0.025)
        recess_mask = 1.0 - smoothstep(-2.0, 2.0, rsdf)
        height -= recess_mask * 0.22

    # Vertical rich emerald-green lacquer gradient
    vt = np.clip((yy - (cy - hh)) / (2.0 * hh), 0.0, 1.0)
    c0 = np.array(color_top, dtype=np.float32)
    c1 = np.array(color_bot, dtype=np.float32)
    albedo = c0[None, None, :] * (1.0 - vt[..., None]) + c1[None, None, :] * vt[..., None]
    if add_recess_window:
        tray_col = np.array([0.07, 0.10, 0.08], dtype=np.float32)
        albedo = albedo * (1.0 - recess_mask[..., None]) + tray_col * recess_mask[..., None]

    rgb = shade_from_height(height, alpha, albedo, metallic=0.22, roughness=0.22, light_dir=(-0.38, -0.52, 0.76), scale=26.0)

    # Top & left metallic clear-coat studio reflection band (gives that crisp glossy green pedal look from Screenshot 1!)
    gloss = np.exp(-((dxn + 0.35) ** 2) * 3.5) * (1.0 - recess_mask * 0.8) * 0.11
    rgb = np.clip(rgb + gloss[..., None], 0.0, 1.0)

    # 4 recessed corner screws
    for sx_s in (-0.82, 0.82):
        for sy_s in (-0.86, 0.86):
            scx = cx + sx_s * hw
            scy = cy + sy_s * hh
            sr = W * 0.022
            d = np.sqrt((xx - scx) ** 2 + (yy - scy) ** 2)
            sm = 1.0 - smoothstep(sr - 1.5, sr + 1.5, d)
            screw_sh = 0.35 + 0.55 * np.clip(1.0 - ((xx - scx) + (yy - scy)) / (sr * 2.2), 0.0, 1.0)
            slot = 1.0 - smoothstep(sr * 0.12, sr * 0.24, np.abs((xx - scx) - (yy - scy))) * (d < sr * 0.72)
            screw_col = np.array([0.78, 0.81, 0.86], dtype=np.float32) * (screw_sh * (0.25 + 0.75 * slot))[..., None]
            rgb = rgb * (1.0 - sm[..., None]) + screw_col * sm[..., None]

    # Soft studio drop shadow
    sh_img = Image.fromarray((alpha * 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=14.0))
    sh = np.asarray(sh_img, dtype=np.float32) / 255.0
    sh_shift = np.zeros_like(sh)
    sh_shift[14:, 4:] = sh[:-14, :-4]

    sh_a = sh_shift * 0.65 * (1.0 - alpha)
    out_a = alpha + sh_a
    out_rgb = np.where(out_a[..., None] > 1e-4, (rgb * alpha[..., None]) / np.maximum(out_a[..., None], 1e-4), 0.0)
    rgba = np.dstack([
        np.clip(out_rgb * 255.0, 0, 255).astype(np.uint8),
        np.clip(out_a * 255.0, 0, 255).astype(np.uint8),
    ])
    img = Image.fromarray(rgba, mode="RGBA").resize((out_w, out_h), Image.Resampling.LANCZOS)
    save_png(img, filename)


# ------------------------------------------------------------------------------
# 4. Contoured Gunmetal CryBaby-Style Wah Pedal Body (matching Screenshot 1 Pedal #5!)
# ------------------------------------------------------------------------------
def render_wah_body():
    out_w, out_h = 300, 480
    S = 2
    W, H = out_w * S, out_h * S
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    cx = W * 0.5
    cy = H * 0.51

    # Classic tapered CryBaby rocker silhouette (wider at toe/top, gently tapered toward heel/bottom)
    ny = (yy - cy) / (H * 0.44)  # -1 at top, +1 at bottom
    half_w_at_y = W * (0.39 - 0.045 * np.clip((ny + 1.0) * 0.5, 0.0, 1.0))
    hh = H * 0.44
    # Signed distance to tapered rounded cast enclosure
    dx = np.abs(xx - cx) - (half_w_at_y - W * 0.085)
    dy = np.abs(yy - cy) - (hh - W * 0.085)
    qx = np.maximum(dx, 0.0)
    qy = np.maximum(dy, 0.0)
    sdf = np.sqrt(qx * qx + qy * qy) + np.minimum(np.maximum(dx, dy), 0.0) - W * 0.085
    alpha = 1.0 - smoothstep(-1.5, 1.5, sdf)

    # Outer sculpted cast-aluminum lip + stepped shoulder + inner recessed rubber treadle bay
    lip_h = smoothstep(0.0, W * 0.055, -sdf)
    height = np.sqrt(np.clip(lip_h * (2.0 - lip_h), 0.0, 1.0)) * 0.92
    # Hammered cast-metal metallic micro-grain
    metal_grain = (fbm_2d(H, W, base_cell=8, octaves=3, seed=77) - 0.5) * 0.018
    height += metal_grain * alpha

    # Inner recessed rubber footplate (lower 74% of rocker, leaving top header for LED + AUTO ON/OFF!)
    pad_cy = H * 0.59
    pad_hh = H * 0.305
    pad_hw_y = W * (0.285 - 0.032 * np.clip((yy - (pad_cy - pad_hh)) / (2.0 * pad_hh), 0.0, 1.0))
    pdx = np.abs(xx - cx) - (pad_hw_y - W * 0.025)
    pdy = np.abs(yy - pad_cy) - (pad_hh - W * 0.025)
    pqx = np.maximum(pdx, 0.0)
    pqy = np.maximum(pdy, 0.0)
    psdf = np.sqrt(pqx * pqx + pqy * pqy) + np.minimum(np.maximum(pdx, pdy), 0.0) - W * 0.025
    pad_mask = 1.0 - smoothstep(-2.0, 2.0, psdf)

    # Vertical rubber grip ribs inside the treadle pad (classic CryBaby longitudinal ribbed rubber like Screenshot 1!)
    rib_freq = 2.0 * math.pi * (xx / 10.5)
    ribs = (0.5 + 0.5 * np.sin(rib_freq)) ** 1.4
    pad_inner = 1.0 - smoothstep(-10.0, -3.0, psdf)
    height = height - pad_mask * 0.22 + (ribs * 0.055) * pad_inner

    # Subtle top recessed well for AUTO switch
    sw_sdf = rounded_rect_sdf(xx, yy, cx, H * 0.165, W * 0.095, H * 0.024, 6.0)
    sw_well = 1.0 - smoothstep(-1.5, 1.5, sw_sdf)
    height -= sw_well * 0.15

    # Gunmetal metallic satin-aluminum albedo vs deep matte black rubber pad (pops crisply on wood deck!)
    shell_grad = 0.44 + 0.18 * (1.0 - np.clip((yy / H), 0.0, 1.0))
    albedo_shell = np.stack([shell_grad * 0.96, shell_grad * 0.98, shell_grad * 1.03], axis=-1)
    albedo_rubber = np.array([0.075, 0.078, 0.085], dtype=np.float32)
    albedo = albedo_shell * (1.0 - pad_mask[..., None]) + albedo_rubber * pad_mask[..., None]
    albedo = albedo * (1.0 - sw_well[..., None] * 0.55)

    # Shade gunmetal shell vs matte rubber separately for true material contrast
    rgb_shell = shade_from_height(height, alpha, albedo, metallic=0.48, roughness=0.26, light_dir=(-0.35, -0.55, 0.75), scale=30.0)
    rgb_rubber = shade_from_height(height, alpha, albedo, metallic=0.05, roughness=0.68, light_dir=(-0.35, -0.55, 0.75), scale=20.0)
    rgb = rgb_shell * (1.0 - pad_mask[..., None]) + rgb_rubber * pad_mask[..., None]

    # Crisp machined outer rim bevel highlight + dark bevel groove around the treadle pad
    outer_rim = (1.0 - smoothstep(2.0, 11.0, np.abs(sdf + 9.0))) * alpha
    rgb = np.clip(rgb + outer_rim[..., None] * 0.14, 0.0, 1.0)
    groove = 1.0 - smoothstep(0.0, 4.5, np.abs(psdf))
    rgb *= (1.0 - 0.55 * groove)[..., None]

    # Drop shadow
    sh_img = Image.fromarray((alpha * 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=16.0))
    sh = np.asarray(sh_img, dtype=np.float32) / 255.0
    sh_shift = np.zeros_like(sh)
    sh_shift[16:, 4:] = sh[:-16, :-4]

    sh_a = sh_shift * 0.70 * (1.0 - alpha)
    out_a = alpha + sh_a
    out_rgb = np.where(out_a[..., None] > 1e-4, (rgb * alpha[..., None]) / np.maximum(out_a[..., None], 1e-4), 0.0)
    rgba = np.dstack([
        np.clip(out_rgb * 255.0, 0, 255).astype(np.uint8),
        np.clip(out_a * 255.0, 0, 255).astype(np.uint8),
    ])
    img = Image.fromarray(rgba, mode="RGBA").resize((out_w, out_h), Image.Resampling.LANCZOS)
    save_png(img, "stomp_wah_body.png")


# ------------------------------------------------------------------------------
# 5. Photorealistic 4x12 Studio Cab + Re-composed 2x12 Vintage Open-Back Cab
# ------------------------------------------------------------------------------
def prepare_cabinets():
    src = Image.open(os.path.join(ART_DIR, "cab_4x12_studio_1795439525170.jpg")).convert("RGB")
    w, h = src.size
    # Tightly crop around the 4x12 cab + boom mic on right (preserving studio floor & mic!)
    # Inspect photo proportions: cabinet spans ~x=11%..89%, y=14%..92%, mic extends to x=100%
    crop_4x12 = src.crop((int(w * 0.08), int(h * 0.12), w, int(h * 0.94)))
    cab4 = crop_4x12.resize((480, 440), Image.Resampling.LANCZOS)

    # Vignette edges cleanly into dark studio transparency so it sits seamlessly in CabRoomView
    arr4 = np.asarray(cab4, dtype=np.float32) / 255.0
    H, W = arr4.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    fade_l = smoothstep(0.0, 26.0, xx)
    fade_t = smoothstep(0.0, 26.0, yy)
    fade_b = smoothstep(0.0, 18.0, H - 1.0 - yy)
    fade_r = smoothstep(0.0, 16.0, W - 1.0 - xx)
    a4 = np.clip(fade_l * fade_t * fade_b * fade_r, 0.0, 1.0)
    rgba4 = np.dstack([
        np.clip(arr4 * 255.0, 0, 255).astype(np.uint8),
        np.clip(a4 * 255.0, 0, 255).astype(np.uint8),
    ])
    save_png(Image.fromarray(rgba4, mode="RGBA"), "cab_4x12_studio.png")

    # 2x12 Vintage Open-Back Boutique Cab (re-composed cleanly from the genuine studio 4x12 photo!)
    # Crop tightly around the cabinet horizontally (0.10..0.90) and take clean top 2 cones + clean bottom piping/tolex border
    x_l, x_r = int(w * 0.10), int(w * 0.90)
    top_part = src.crop((x_l, int(h * 0.13), x_r, int(h * 0.515)))
    bot_trim = src.crop((x_l, int(h * 0.855), x_r, int(h * 0.93)))
    comb_h = top_part.height + bot_trim.height
    comb_w = top_part.width
    cab2_raw = Image.new("RGB", (comb_w, comb_h))
    cab2_raw.paste(top_part, (0, 0))
    cab2_raw.paste(bot_trim, (0, top_part.height))
    c2 = np.asarray(cab2_raw, dtype=np.float32) / 255.0
    seam = top_part.height
    for dy in range(-3, 4):
        yy_s = seam + dy
        if 0 <= yy_s < comb_h:
            t = (dy + 3) / 6.0
            c2[yy_s] = c2[seam - 4] * (1.0 - t) + c2[seam + 4] * t

    # Re-grade to warm vintage chocolate-brown tolex & wheat-gold grill cloth!
    luma2 = 0.299 * c2[..., 0] + 0.587 * c2[..., 1] + 0.114 * c2[..., 2]
    # Apply rich warm vintage tweed/brown color grading (preserves 100% genuine tolex leather & grill weave!)
    warm = np.empty_like(c2)
    warm[..., 0] = np.clip(c2[..., 0] * 1.24 + 0.055 * smoothstep(0.06, 0.35, luma2), 0.0, 1.0)
    warm[..., 1] = np.clip(c2[..., 1] * 1.02 + 0.025 * smoothstep(0.06, 0.35, luma2), 0.0, 1.0)
    warm[..., 2] = np.clip(c2[..., 2] * 0.72, 0.0, 1.0)

    cab2_img = Image.fromarray((warm * 255.0).astype(np.uint8), mode="RGB").resize((520, 320), Image.Resampling.LANCZOS)
    arr2 = np.asarray(cab2_img, dtype=np.float32) / 255.0
    H2, W2 = arr2.shape[:2]
    yy2, xx2 = np.mgrid[0:H2, 0:W2].astype(np.float32)
    a2 = (
        smoothstep(0.0, 24.0, xx2)
        * smoothstep(0.0, 22.0, yy2)
        * smoothstep(0.0, 16.0, H2 - 1.0 - yy2)
        * smoothstep(0.0, 16.0, W2 - 1.0 - xx2)
    )
    rgba2 = np.dstack([
        np.clip(arr2 * 255.0, 0, 255).astype(np.uint8),
        np.clip(a2 * 255.0, 0, 255).astype(np.uint8),
    ])
    save_png(Image.fromarray(rgba2, mode="RGBA"), "cab_2x12_open.png")


# ------------------------------------------------------------------------------
# 6. Photorealistic Tube Amp Head Chassis (Pebbled Tolex + Chrome Corners + Leather Handle)
# ------------------------------------------------------------------------------
def render_amp_head_chassis():
    out_w, out_h = 960, 380
    S = 2
    W, H = out_w * S, out_h * S
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    cx, cy = W * 0.5, H * 0.53
    hw, hh = W * 0.47, H * 0.415
    r = 26.0

    # Main cabinet SDF
    cab_sdf = rounded_rect_sdf(xx, yy, cx, cy, hw, hh, r)
    cab_alpha = 1.0 - smoothstep(-1.5, 1.5, cab_sdf)

    # Top leather handle + chrome end caps
    h_cy = cy - hh - 18.0
    handle_sdf = rounded_rect_sdf(xx, yy, cx, h_cy, W * 0.115, 14.0, 10.0)
    handle_alpha = (1.0 - smoothstep(-1.5, 1.5, handle_sdf)) * (1.0 - cab_alpha * 0.5)
    cap_l_sdf = rounded_rect_sdf(xx, yy, cx - W * 0.115, h_cy + 10.0, 22.0, 15.0, 5.0)
    cap_r_sdf = rounded_rect_sdf(xx, yy, cx + W * 0.115, h_cy + 10.0, 22.0, 15.0, 5.0)
    cap_alpha = np.maximum(1.0 - smoothstep(-1.5, 1.5, cap_l_sdf), 1.0 - smoothstep(-1.5, 1.5, cap_r_sdf))

    total_alpha = np.clip(np.maximum(cab_alpha, np.maximum(handle_alpha, cap_alpha)), 0.0, 1.0)

    # Pebbled black leather tolex heightfield (rich high-frequency leather grain!)
    pebble1 = fbm_2d(H, W, base_cell=12, octaves=3, seed=501)
    pebble2 = value_noise_2d(H, W, cell_size=5, seed=509)
    tolex_h = 0.065 * np.sin(pebble1 * 12.0) + 0.045 * pebble2
    bevel = smoothstep(0.0, 30.0, -cab_sdf)
    height = (0.85 * np.sqrt(np.clip(bevel * (2.0 - bevel), 0.0, 1.0)) + tolex_h) * cab_alpha

    # Inner recessed baffle cavity (where tubes & faceplate sit!)
    in_sdf = rounded_rect_sdf(xx, yy, cx, cy, hw - 40.0, hh - 34.0, 14.0)
    in_mask = 1.0 - smoothstep(-2.0, 2.0, in_sdf)
    height -= in_mask * 0.35

    # Shade outer pebbled tolex
    tolex_albedo = np.full((H, W, 3), [0.105, 0.112, 0.125], dtype=np.float32)
    rgb = shade_from_height(height, cab_alpha, tolex_albedo, metallic=0.14, roughness=0.42, light_dir=(-0.3, -0.65, 0.7), scale=22.0)

    # Recessed interior dark chamber with subtle metallic mesh texture
    mesh_x = np.abs(np.sin((xx + yy) * 0.32))
    mesh_y = np.abs(np.sin((xx - yy) * 0.32))
    mesh = smoothstep(0.65, 0.95, np.maximum(mesh_x, mesh_y))
    chamber_col = np.array([0.035, 0.040, 0.050], dtype=np.float32) + mesh[..., None] * 0.025
    rgb = rgb * (1.0 - in_mask[..., None]) + chamber_col * in_mask[..., None]

    # Silver/cream piping trim ring around inner baffle (thin crisp ring ONLY at in_sdf ~= 4)
    piping = (1.0 - smoothstep(1.5, 5.5, np.abs(in_sdf - 4.0))) * cab_alpha
    pipe_col = np.array([0.84, 0.80, 0.72], dtype=np.float32)
    rgb = rgb * (1.0 - piping[..., None] * 0.85) + pipe_col * (piping[..., None] * 0.85)

    # 4 Brushed steel corner protectors with rivets
    corners = [
        (cx - hw, cy - hh, 1.0, 1.0),
        (cx + hw, cy - hh, -1.0, 1.0),
        (cx - hw, cy + hh, 1.0, -1.0),
        (cx + hw, cy + hh, -1.0, -1.0),
    ]
    for kx, ky, sx, sy in corners:
        lx = (xx - kx) * sx
        ly = (yy - ky) * sy
        d_corner = np.maximum(lx, ly)
        c_mask = (
            (lx >= 0) & (ly >= 0)
            & (lx + ly < 84.0)
            & (d_corner < 64.0)
        ).astype(np.float32) * cab_alpha
        # Brushed steel sheen
        br = 0.25 + 0.35 * np.clip(1.0 - (lx + ly) / 90.0, 0.0, 1.0)
        c_col = np.stack([br * 0.95, br * 0.98, br * 1.05], axis=-1)
        rgb = rgb * (1.0 - c_mask[..., None]) + c_col * c_mask[..., None]
        # Rivet head
        rd = np.sqrt((lx - 24.0) ** 2 + (ly - 24.0) ** 2)
        rm = 1.0 - smoothstep(5.0, 7.0, rd)
        rgb = rgb * (1.0 - rm[..., None]) + np.array([0.75, 0.78, 0.82], dtype=np.float32) * rm[..., None]

    # Top stitched leather handle & chrome brackets
    h_shade = 0.16 + 0.16 * np.clip(1.0 - (yy - (h_cy - 14.0)) / 28.0, 0.0, 1.0)
    rgb = rgb * (1.0 - handle_alpha[..., None]) + np.stack([h_shade, h_shade * 0.92, h_shade * 0.86], axis=-1) * handle_alpha[..., None]
    cap_shade = 0.35 + 0.55 * np.clip(1.0 - (yy - (h_cy - 5.0)) / 28.0, 0.0, 1.0)
    rgb = rgb * (1.0 - cap_alpha[..., None]) + np.stack([cap_shade * 0.94, cap_shade * 0.97, cap_shade], axis=-1) * cap_alpha[..., None]

    # Soft studio shadow
    sh_img = Image.fromarray((total_alpha * 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=16.0))
    sh = np.asarray(sh_img, dtype=np.float32) / 255.0
    sh_shift = np.zeros_like(sh)
    sh_shift[16:, :] = sh[:-16, :]
    sh_a = sh_shift * 0.68 * (1.0 - total_alpha)
    out_a = total_alpha + sh_a
    out_rgb = np.where(out_a[..., None] > 1e-4, (rgb * total_alpha[..., None]) / np.maximum(out_a[..., None], 1e-4), 0.0)
    rgba = np.dstack([
        np.clip(out_rgb * 255.0, 0, 255).astype(np.uint8),
        np.clip(out_a * 255.0, 0, 255).astype(np.uint8),
    ])
    img = Image.fromarray(rgba, mode="RGBA").resize((out_w, out_h), Image.Resampling.LANCZOS)
    save_png(img, "amp_head_chassis.png")


# ------------------------------------------------------------------------------
# 7. 2-Frame 3D PBR Chrome Bat-Handle Toggle Switch (64x128: Top=UP/ON, Bot=DOWN/OFF)
# ------------------------------------------------------------------------------
def render_chrome_toggle_2():
    fw, fh = 64, 64
    S = 2
    W, H = fw * S, fh * S
    strip = Image.new("RGBA", (fw, fh * 2), (0, 0, 0, 0))

    for frame_idx, is_up in enumerate([True, False]):
        yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
        cx, cy = W * 0.5, H * 0.52

        # Outer hex nut + chrome washer
        dx = xx - cx
        dy = yy - cy
        r = np.sqrt(dx * dx + dy * dy)
        ang = np.arctan2(dy, dx)
        hex_r = 32.0 / np.maximum(np.cos(((ang + math.pi / 6.0) % (math.pi / 3.0)) - math.pi / 6.0), 0.5)
        hex_mask = 1.0 - smoothstep(hex_r - 2.0, hex_r + 1.5, r)
        ring_mask = 1.0 - smoothstep(22.0, 24.5, r)
        hole_mask = 1.0 - smoothstep(13.0, 15.0, r)

        # Chrome specular shading for nut/collar
        nut_l = 0.42 + 0.48 * np.clip(0.5 - 0.5 * (dx * 0.6 + dy * 0.8) / 36.0, 0.0, 1.0)
        nut_col = np.stack([nut_l * 0.92, nut_l * 0.96, nut_l], axis=-1)
        hole_col = np.full((H, W, 3), [0.06, 0.07, 0.09], dtype=np.float32)

        rgb = nut_col * hex_mask[..., None]
        rgb = rgb * (1.0 - ring_mask[..., None] * 0.15)
        rgb = rgb * (1.0 - hole_mask[..., None]) + hole_col * hole_mask[..., None]
        alpha = hex_mask.copy()

        # Metallic bat lever extending UP (frame 0) or DOWN (frame 1)
        dir_y = -1.0 if is_up else 1.0
        tip_y = cy + dir_y * 36.0
        # Lever shaft trapezoid + rounded chrome bulb tip
        t_y = np.clip((yy - cy) * dir_y / 36.0, 0.0, 1.0)
        lever_hw = 7.0 + 7.5 * t_y
        in_y = ((yy - cy) * dir_y >= -2.0) & ((yy - cy) * dir_y <= 36.0)
        shaft_d = np.abs(dx) - lever_hw
        shaft_m = (1.0 - smoothstep(-1.5, 1.5, shaft_d)) * in_y.astype(np.float32)
        bulb_d = np.sqrt(dx * dx + (yy - tip_y) ** 2) - 14.5
        bulb_m = 1.0 - smoothstep(-1.5, 1.5, bulb_d)
        lever_m = np.maximum(shaft_m, bulb_m)

        # Cylindrical chrome reflection across lever width
        cyl_nx = np.clip(dx / np.maximum(lever_hw, 1.0), -1.0, 1.0)
        cyl_nz = np.sqrt(np.clip(1.0 - cyl_nx * cyl_nx, 0.0, 1.0))
        cyl_spec = np.clip(0.25 + 0.75 * cyl_nz - 0.35 * cyl_nx, 0.0, 1.0)
        cyl_spec = np.clip(cyl_spec + 0.45 * (np.clip(0.85 * cyl_nz - 0.52 * cyl_nx, 0.0, 1.0) ** 14.0), 0.0, 1.0)
        lever_col = np.stack([cyl_spec * 0.94, cyl_spec * 0.97, cyl_spec], axis=-1)

        # Shadow cast by lever onto hex nut
        sh_y = yy - dir_y * 7.0
        sh_d = np.sqrt((dx - 4.0) ** 2 + (sh_y - tip_y) ** 2) - 14.0
        lev_sh = (1.0 - smoothstep(-4.0, 8.0, sh_d)) * 0.45 * hex_mask * (1.0 - lever_m)
        rgb *= (1.0 - lev_sh)[..., None]

        rgb = rgb * (1.0 - lever_m[..., None]) + lever_col * lever_m[..., None]
        alpha = np.maximum(alpha, lever_m)

        rgba = np.dstack([
            np.clip(rgb * 255.0, 0, 255).astype(np.uint8),
            np.clip(alpha * 255.0, 0, 255).astype(np.uint8),
        ])
        frame = Image.fromarray(rgba, mode="RGBA").resize((fw, fh), Image.Resampling.LANCZOS)
        strip.paste(frame, (0, frame_idx * fh))

    save_png(strip, "toggle_chrome_2.png")


# ------------------------------------------------------------------------------
# 8. Crisp Studio Rack Unit Thumbnails (for Bottom Signal Chain & Right Gear Sidebar!)
# ------------------------------------------------------------------------------
def render_rack_thumbnail(filename, face_rgb, accent_rgb, has_lcd=False, num_knobs=4):
    W, H = 180, 76
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Outer rack ears
    d.rounded_rectangle([2, 6, W - 3, H - 7], radius=5, fill=(18, 21, 28, 255), outline=(65, 75, 92, 255), width=1)
    # Brushed faceplate
    d.rounded_rectangle([14, 8, W - 15, H - 9], radius=3, fill=face_rgb + (255,), outline=(95, 108, 128, 200), width=1)
    # Rack ear screw holes
    for ex in (8, W - 9):
        for ey in (18, H - 19):
            d.ellipse([ex - 3, ey - 3, ex + 3, ey + 3], fill=(120, 130, 145, 255), outline=(10, 12, 16, 255))
    # Glowing accent rail + LED
    d.rectangle([18, 13, 21, H - 14], fill=accent_rgb + (255,))
    d.ellipse([27, 20, 35, 28], fill=accent_rgb + (255,), outline=(255, 255, 255, 200))

    x_start = 44
    if has_lcd:
        d.rounded_rectangle([42, 16, 86, H - 17], radius=3, fill=(8, 12, 18, 255), outline=accent_rgb + (200,), width=1)
        d.line([48, 36, 80, 36], fill=(251, 191, 36, 255), width=3)
        x_start = 94

    # Mini brushed studio knobs
    avail = (W - 22) - x_start
    step = avail // max(1, num_knobs)
    for i in range(num_knobs):
        kx = x_start + i * step + step // 2
        ky = H // 2
        kr = 10
        d.ellipse([kx - kr, ky - kr + 2, kx + kr, ky + kr + 2], fill=(0, 0, 0, 150))
        d.ellipse([kx - kr, ky - kr, kx + kr, ky + kr], fill=(42, 48, 60, 255), outline=(145, 160, 180, 220), width=1)
        d.line([kx, ky, kx + 4, ky - 7], fill=(240, 245, 255, 255), width=2)

    save_png(img, filename)


if __name__ == "__main__":
    print("Preparing photorealistic skin assets for PedalBoard v0.5.0...")
    prepare_wood_deck()
    prepare_diecast_stompboxes()
    render_boutique_green_pedal(320, 480, "stomp_eq_green.png",
                                color_top=(0.06, 0.56, 0.14), color_bot=(0.02, 0.30, 0.08),
                                add_recess_window=True)
    render_boutique_green_pedal(240, 380, "stomp_chorus_green.png",
                                color_top=(0.05, 0.64, 0.14), color_bot=(0.02, 0.34, 0.07),
                                add_recess_window=False)
    render_wah_body()
    prepare_cabinets()
    render_amp_head_chassis()
    render_chrome_toggle_2()
    render_rack_thumbnail("rack_chorus_thumb.png", (30, 42, 68), (99, 102, 241), has_lcd=False, num_knobs=4)
    render_rack_thumbnail("rack_delay_thumb.png",  (26, 44, 66), (59, 130, 246), has_lcd=True,  num_knobs=3)
    render_rack_thumbnail("rack_reverb_thumb.png", (42, 32, 60), (168, 85, 247), has_lcd=False, num_knobs=3)
    print("All hardware plates generated successfully!")
