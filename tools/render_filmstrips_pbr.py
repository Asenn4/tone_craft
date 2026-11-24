"""
Fast, zero-drift 3D PBR-shaded supersampled filmstrip generator for PedalBoard v0.5.0.
Renders 64-frame vertical RGBA PNG filmstrips (2x supersampled -> Lanczos 96x96)
with fixed studio top-left key light, cool rim light, anisotropic brushed metal caps,
real fluted/knurled 3D normals, and fixed cast ground shadows.
"""
import math
import os
import numpy as np
from PIL import Image

OUT_DIR = r"D:\Self Project\pedal_board\assets\skins\default"
os.makedirs(OUT_DIR, exist_ok=True)


def normalize3(x, y, z):
    n = np.sqrt(x * x + y * y + z * z) + 1e-8
    return x / n, y / n, z / n


def shade_pbr(nx, ny, nz, base_rgb, metallic, roughness, alpha_mask, radial_angle=None, aniso=0.0):
    # Fixed studio lighting (top-left warm key, bottom-right cool rim, overhead soft fill)
    kx, ky, kz = normalize3(-0.52, -0.58, 0.63)
    rx, ry, rz = normalize3(0.62, 0.56, 0.55)
    vx, vy, vz = 0.0, 0.0, 1.0

    # Half vectors
    hkx, hky, hkz = normalize3(kx + vx, ky + vy, kz + vz)
    hrx, hry, hrz = normalize3(rx + vx, ry + vy, rz + vz)

    ndotl_k = np.clip(nx * kx + ny * ky + nz * kz, 0.0, 1.0)
    ndotl_r = np.clip(nx * rx + ny * ry + nz * rz, 0.0, 1.0)
    ndoth_k = np.clip(nx * hkx + ny * hky + nz * hkz, 0.0, 1.0)
    ndoth_r = np.clip(nx * hrx + ny * hry + nz * hrz, 0.0, 1.0)

    shininess = max(6.0, 2.0 / (max(0.06, roughness) ** 2) - 2.0)
    spec_k = (ndoth_k ** shininess) * (0.45 + 1.15 * metallic)
    spec_r = (ndoth_r ** (shininess * 0.6)) * (0.20 + 0.55 * metallic)

    # Radial brushed anisotropy modulation on metallic top caps
    aniso_mod = 1.0
    if radial_angle is not None and aniso > 0.0:
        aniso_mod = 1.0 + aniso * (0.65 * np.cos(2.0 * radial_angle + 0.85) + 0.35 * np.cos(4.0 * radial_angle - 0.4))

    r0, g0, b0 = base_rgb
    diffuse_scale = (1.0 - 0.65 * metallic)
    ambient = 0.22 + 0.14 * np.clip(nz, 0.0, 1.0)

    # Key light warm tint, rim light cool tint
    lit_r = r0 * (ambient + diffuse_scale * (1.05 * ndotl_k + 0.32 * ndotl_r)) * aniso_mod
    lit_g = g0 * (ambient + diffuse_scale * (1.02 * ndotl_k + 0.36 * ndotl_r)) * aniso_mod
    lit_b = b0 * (ambient + diffuse_scale * (0.96 * ndotl_k + 0.44 * ndotl_r)) * aniso_mod

    # Specular reflection tint (metals tint specular by base color, dielectrics reflect white)
    sr = (1.0 - metallic) + metallic * min(1.0, r0 * 1.25 + 0.15)
    sg = (1.0 - metallic) + metallic * min(1.0, g0 * 1.25 + 0.15)
    sb = (1.0 - metallic) + metallic * min(1.0, b0 * 1.25 + 0.15)

    out_r = np.clip(lit_r + spec_k * sr * 1.00 + spec_r * sr * 0.42, 0.0, 1.0)
    out_g = np.clip(lit_g + spec_k * sg * 0.98 + spec_r * sg * 0.48, 0.0, 1.0)
    out_b = np.clip(lit_b + spec_k * sb * 0.94 + spec_r * sb * 0.56, 0.0, 1.0)

    rgba = np.zeros((*nx.shape, 4), dtype=np.float32)
    rgba[..., 0] = out_r
    rgba[..., 1] = out_g
    rgba[..., 2] = out_b
    rgba[..., 3] = np.clip(alpha_mask, 0.0, 1.0)
    return rgba


def alpha_over(dst, src):
    sa = src[..., 3:4]
    da = dst[..., 3:4]
    out_a = sa + da * (1.0 - sa)
    safe_a = np.where(out_a > 1e-6, out_a, 1.0)
    out_rgb = (src[..., :3] * sa + dst[..., :3] * da * (1.0 - sa)) / safe_a
    return np.concatenate([out_rgb, out_a], axis=-1)


def render_knob_frame(size, angle_rad, style):
    # Render at 2x supersampling then Lanczos downsample for razor-sharp edges
    ss = size * 2
    lin = np.linspace(-1.18, 1.18, ss, dtype=np.float32)
    xx, yy = np.meshgrid(lin, lin)
    rr = np.sqrt(xx * xx + yy * yy)
    theta = np.arctan2(yy, xx)  # 0 = +X, -pi/2 = 12 o'clock (-Y in image coords)

    # Fixed soft ambient drop shadow offset down-right
    s_rr = np.sqrt((xx - 0.06) ** 2 + (yy - 0.09) ** 2)
    shadow_a = np.clip((1.08 - s_rr) / 0.24, 0.0, 1.0) ** 1.8 * 0.62
    canvas = np.zeros((ss, ss, 4), dtype=np.float32)
    canvas[..., 3] = shadow_a

    # Local rotated coordinates (angle_rad = 0 means pointer points UP = -Y)
    ca = math.cos(angle_rad)
    sa = math.sin(angle_rad)
    lx = xx * ca + yy * sa
    ly = -xx * sa + yy * ca
    ltheta = np.arctan2(ly, lx)

    if style in ("vintage_skirted", "gold_plexi"):
        # Outer tapered skirt (r <= 0.94)
        skirt_mask = np.clip((0.94 - rr) / 0.025, 0.0, 1.0)
        slope = np.clip((rr - 0.68) / 0.26, 0.0, 1.0)
        flute = 0.22 * np.sin(12.0 * ltheta) * np.clip((rr - 0.56) / 0.22, 0.0, 1.0)
        nx = (xx / (rr + 1e-6)) * (0.45 * slope) - (yy / (rr + 1e-6)) * flute
        ny = (yy / (rr + 1e-6)) * (0.45 * slope) + (xx / (rr + 1e-6)) * flute
        nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.05, 1.0))
        skirt_rgba = shade_pbr(nx, ny, nz, (0.11, 0.12, 0.14), metallic=0.10, roughness=0.28, alpha_mask=skirt_mask)
        canvas = alpha_over(canvas, skirt_rgba)

        # Raised fluted inner cylinder (r <= 0.74 with subtle 12-lobe edge)
        lobe_r = 0.75 + 0.032 * np.cos(12.0 * ltheta)
        core_mask = np.clip((lobe_r - rr) / 0.02, 0.0, 1.0)
        rim_slope = np.clip((rr - 0.62) / 0.15, 0.0, 1.0)
        cnx = (xx / (rr + 1e-6)) * (0.65 * rim_slope) - (yy / (rr + 1e-6)) * (0.30 * np.sin(12.0 * ltheta) * rim_slope)
        cny = (yy / (rr + 1e-6)) * (0.65 * rim_slope) + (xx / (rr + 1e-6)) * (0.30 * np.sin(12.0 * ltheta) * rim_slope)
        cnz = np.sqrt(np.clip(1.0 - cnx * cnx - cny * cny, 0.08, 1.0))
        core_rgba = shade_pbr(cnx, cny, cnz, (0.08, 0.085, 0.10), metallic=0.12, roughness=0.24, alpha_mask=core_mask)
        canvas = alpha_over(canvas, core_rgba)

        # Brushed metallic top cap (Gold for Plexi, Silver Chrome for Vintage Skirted)
        cap_mask = np.clip((0.55 - rr) / 0.018, 0.0, 1.0)
        bevel = np.clip((rr - 0.47) / 0.08, 0.0, 1.0)
        knx = (xx / (rr + 1e-6)) * (0.38 * bevel)
        kny = (yy / (rr + 1e-6)) * (0.38 * bevel)
        knz = np.sqrt(np.clip(1.0 - knx * knx - kny * kny, 0.2, 1.0))
        cap_rgb = (0.88, 0.68, 0.28) if style == "gold_plexi" else (0.78, 0.82, 0.87)
        # Subtle concentric brushed rings
        ring_noise = 1.0 + 0.04 * np.sin(rr * 85.0)
        cap_rgba = shade_pbr(knx, kny, knz, cap_rgb, metallic=0.92, roughness=0.24,
                             alpha_mask=cap_mask, radial_angle=theta, aniso=0.22)
        cap_rgba[..., :3] = np.clip(cap_rgba[..., :3] * ring_noise[..., None], 0.0, 1.0)
        canvas = alpha_over(canvas, cap_rgba)

        # Crisp pointer line on top pointing toward -Y in local coords (12 o'clock at 0 rad)
        ptr_x = np.clip((0.055 - np.abs(lx)) / 0.014, 0.0, 1.0)
        ptr_y = np.clip((-0.14 - ly) / 0.018, 0.0, 1.0) * np.clip((ly + 0.82) / 0.018, 0.0, 1.0)
        ptr_mask = ptr_x * ptr_y * np.clip((0.86 - rr) / 0.02, 0.0, 1.0)
        ptr_col = (0.06, 0.06, 0.07) if style == "gold_plexi" else (0.96, 0.98, 1.0)
        ptr_rgba = np.zeros((ss, ss, 4), dtype=np.float32)
        ptr_rgba[..., 0], ptr_rgba[..., 1], ptr_rgba[..., 2] = ptr_col
        ptr_rgba[..., 3] = ptr_mask
        canvas = alpha_over(canvas, ptr_rgba)

    elif style == "chrome_knurled":
        # Heavy knurled chrome/gunmetal cylinder
        knurl_r = 0.88 + 0.022 * np.cos(28.0 * ltheta)
        body_mask = np.clip((knurl_r - rr) / 0.018, 0.0, 1.0)
        edge = np.clip((rr - 0.72) / 0.16, 0.0, 1.0)
        k_bump = 0.42 * np.sin(28.0 * ltheta) * edge
        nx = (xx / (rr + 1e-6)) * (0.55 * edge) - (yy / (rr + 1e-6)) * k_bump
        ny = (yy / (rr + 1e-6)) * (0.55 * edge) + (xx / (rr + 1e-6)) * k_bump
        nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.1, 1.0))
        body_rgba = shade_pbr(nx, ny, nz, (0.58, 0.62, 0.68), metallic=0.95, roughness=0.22,
                              alpha_mask=body_mask, radial_angle=theta, aniso=0.18)
        canvas = alpha_over(canvas, body_rgba)

        # Dark gunmetal top recess
        top_mask = np.clip((0.70 - rr) / 0.018, 0.0, 1.0)
        bev = np.clip((rr - 0.61) / 0.09, 0.0, 1.0)
        tnx = (xx / (rr + 1e-6)) * (-0.30 * bev)
        tny = (yy / (rr + 1e-6)) * (-0.30 * bev)
        tnz = np.sqrt(np.clip(1.0 - tnx * tnx - tny * tny, 0.2, 1.0))
        top_rgba = shade_pbr(tnx, tny, tnz, (0.19, 0.22, 0.27), metallic=0.82, roughness=0.28,
                             alpha_mask=top_mask, radial_angle=theta, aniso=0.15)
        canvas = alpha_over(canvas, top_rgba)

        # Glowing crimson pointer notch
        ptr_x = np.clip((0.052 - np.abs(lx)) / 0.014, 0.0, 1.0)
        ptr_y = np.clip((-0.10 - ly) / 0.018, 0.0, 1.0) * np.clip((ly + 0.66) / 0.018, 0.0, 1.0)
        ptr_rgba = np.zeros((ss, ss, 4), dtype=np.float32)
        ptr_rgba[..., 0], ptr_rgba[..., 1], ptr_rgba[..., 2] = (0.98, 0.22, 0.26)
        ptr_rgba[..., 3] = ptr_x * ptr_y
        canvas = alpha_over(canvas, ptr_rgba)

    else:  # cyan_stomp (clean satin sky-blue boutique knob like screenshot reference!)
        lobe_r = 0.84 + 0.025 * np.cos(8.0 * ltheta)
        body_mask = np.clip((lobe_r - rr) / 0.02, 0.0, 1.0)
        slope = np.clip((rr - 0.58) / 0.26, 0.0, 1.0)
        nx = (xx / (rr + 1e-6)) * (0.52 * slope) - (yy / (rr + 1e-6)) * (0.24 * np.sin(8.0 * ltheta) * slope)
        ny = (yy / (rr + 1e-6)) * (0.52 * slope) + (xx / (rr + 1e-6)) * (0.24 * np.sin(8.0 * ltheta) * slope)
        nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.12, 1.0))
        body_rgba = shade_pbr(nx, ny, nz, (0.16, 0.68, 0.96), metallic=0.15, roughness=0.25, alpha_mask=body_mask)
        canvas = alpha_over(canvas, body_rgba)

        # White pointer indicator line
        ptr_x = np.clip((0.055 - np.abs(lx)) / 0.014, 0.0, 1.0)
        ptr_y = np.clip((-0.14 - ly) / 0.018, 0.0, 1.0) * np.clip((ly + 0.72) / 0.018, 0.0, 1.0)
        ptr_rgba = np.zeros((ss, ss, 4), dtype=np.float32)
        ptr_rgba[..., 0], ptr_rgba[..., 1], ptr_rgba[..., 2] = (0.98, 0.99, 1.0)
        ptr_rgba[..., 3] = ptr_x * ptr_y
        canvas = alpha_over(canvas, ptr_rgba)

    img = Image.fromarray((np.clip(canvas, 0.0, 1.0) * 255.0).astype(np.uint8), "RGBA")
    return img.resize((size, size), Image.Resampling.LANCZOS)


def build_knob_strip(filename, style, frames=64, size=96):
    strip = Image.new("RGBA", (size, size * frames), (0, 0, 0, 0))
    for i in range(frames):
        t = i / float(frames - 1)
        # Sweep from -135 deg (min) clockwise to +135 deg (max)
        angle_rad = math.radians(-135.0 + t * 270.0)
        frame = render_knob_frame(size, angle_rad, style)
        strip.paste(frame, (0, i * size))
    out = os.path.join(OUT_DIR, filename)
    strip.save(out, optimize=True)
    print(f"OK: {filename} ({strip.size}, {os.path.getsize(out):,} B)")


def build_footswitch_strip(size=96):
    ss = size * 2
    lin = np.linspace(-1.18, 1.18, ss, dtype=np.float32)
    xx, yy = np.meshgrid(lin, lin)
    rr = np.sqrt(xx * xx + yy * yy)
    theta = np.arctan2(yy, xx)

    strip = Image.new("RGBA", (size, size * 2), (0, 0, 0, 0))
    for idx, pressed in enumerate((False, True)):
        canvas = np.zeros((ss, ss, 4), dtype=np.float32)
        s_rr = np.sqrt((xx - 0.05) ** 2 + (yy - 0.08) ** 2)
        canvas[..., 3] = np.clip((1.05 - s_rr) / 0.22, 0.0, 1.0) ** 1.8 * 0.65

        # Outer steel washer
        w_mask = np.clip((0.96 - rr) / 0.02, 0.0, 1.0)
        wnx = (xx / (rr + 1e-6)) * 0.18 * np.clip((rr - 0.80) / 0.16, 0.0, 1.0)
        wny = (yy / (rr + 1e-6)) * 0.18 * np.clip((rr - 0.80) / 0.16, 0.0, 1.0)
        wnz = np.sqrt(np.clip(1.0 - wnx * wnx - wny * wny, 0.2, 1.0))
        canvas = alpha_over(canvas, shade_pbr(wnx, wny, wnz, (0.62, 0.66, 0.72), 0.92, 0.26, w_mask, theta, 0.20))

        # Hexagonal chrome nut (6-sided distance function)
        a_mod = ((theta + math.pi) % (math.pi / 3.0)) - (math.pi / 6.0)
        hex_r = rr * np.cos(a_mod)
        h_mask = np.clip((0.72 - hex_r) / 0.018, 0.0, 1.0)
        h_edge = np.clip((hex_r - 0.60) / 0.12, 0.0, 1.0)
        hnx = (xx / (rr + 1e-6)) * 0.48 * h_edge
        hny = (yy / (rr + 1e-6)) * 0.48 * h_edge
        hnz = np.sqrt(np.clip(1.0 - hnx * hnx - hny * hny, 0.15, 1.0))
        canvas = alpha_over(canvas, shade_pbr(hnx, hny, hnz, (0.78, 0.82, 0.87), 0.96, 0.18, h_mask, theta, 0.15))

        # Dark threaded collar shadow
        col_mask = np.clip((0.52 - rr) / 0.018, 0.0, 1.0)
        canvas = alpha_over(canvas, shade_pbr(np.zeros_like(rr), np.zeros_like(rr), np.ones_like(rr),
                                              (0.18, 0.20, 0.24), 0.5, 0.45, col_mask))

        # Domed chrome steel plunger (slightly smaller & darker when pressed down)
        pr = 0.43 if pressed else 0.46
        p_mask = np.clip((pr - rr) / 0.016, 0.0, 1.0)
        pnx = np.clip(xx / pr, -1.0, 1.0) * 0.55
        pny = np.clip(yy / pr, -1.0, 1.0) * 0.55
        pnz = np.sqrt(np.clip(1.0 - pnx * pnx - pny * pny, 0.2, 1.0))
        p_col = (0.70, 0.74, 0.80) if pressed else (0.88, 0.91, 0.96)
        canvas = alpha_over(canvas, shade_pbr(pnx, pny, pnz, p_col, 0.98, 0.14, p_mask, theta, 0.12))

        im = Image.fromarray((np.clip(canvas, 0.0, 1.0) * 255.0).astype(np.uint8), "RGBA")
        strip.paste(im.resize((size, size), Image.Resampling.LANCZOS), (0, idx * size))

    out = os.path.join(OUT_DIR, "footswitch_chrome_2.png")
    strip.save(out, optimize=True)
    print(f"OK: footswitch_chrome_2.png ({strip.size}, {os.path.getsize(out):,} B)")


def build_console_fader_cap():
    w, h = 64, 48
    sw, sh = w * 2, h * 2
    x = np.linspace(-1.1, 1.1, sw, dtype=np.float32)
    y = np.linspace(-1.1, 1.1, sh, dtype=np.float32)
    xx, yy = np.meshgrid(x, y)

    canvas = np.zeros((sh, sw, 4), dtype=np.float32)
    # Shadow
    sd = np.maximum(np.abs(xx - 0.05) - 0.86, np.abs(yy - 0.09) - 0.78)
    canvas[..., 3] = np.clip((0.15 - sd) / 0.18, 0.0, 1.0) * 0.65

    # Brushed concave aluminum console fader cap
    d = np.maximum(np.abs(xx) - 0.86, np.abs(yy) - 0.78)
    mask = np.clip((0.06 - d) / 0.04, 0.0, 1.0)
    # Concave finger valley along Y
    nx = np.clip(xx * 0.18, -0.5, 0.5)
    ny = np.clip(-yy * 0.72, -0.8, 0.8)
    nz = np.sqrt(np.clip(1.0 - nx * nx - ny * ny, 0.2, 1.0))
    cap = shade_pbr(nx, ny, nz, (0.76, 0.80, 0.86), 0.90, 0.24, mask)
    canvas = alpha_over(canvas, cap)

    # Center matte black index line
    line_m = mask * np.clip((0.10 - np.abs(yy)) / 0.03, 0.0, 1.0) * np.clip((0.84 - np.abs(xx)) / 0.04, 0.0, 1.0)
    stripe = np.zeros((sh, sw, 4), dtype=np.float32)
    stripe[..., :3] = 0.04
    stripe[..., 3] = line_m
    canvas = alpha_over(canvas, stripe)

    im = Image.fromarray((np.clip(canvas, 0.0, 1.0) * 255.0).astype(np.uint8), "RGBA")
    out = os.path.join(OUT_DIR, "fader_console_cap.png")
    im.resize((w, h), Image.Resampling.LANCZOS).save(out, optimize=True)
    print(f"OK: fader_console_cap.png ({os.path.getsize(out):,} B)")


if __name__ == "__main__":
    build_knob_strip("knob_vintage_skirted_64.png", "vintage_skirted", frames=64, size=96)
    build_knob_strip("knob_gold_plexi_64.png", "gold_plexi", frames=64, size=96)
    build_knob_strip("knob_chrome_knurled_64.png", "chrome_knurled", frames=64, size=96)
    build_knob_strip("knob_cyan_stomp_64.png", "cyan_stomp", frames=64, size=96)
    build_footswitch_strip(size=96)
    build_console_fader_cap()
    print("ALL 3D PBR FILMSTRIPS GENERATED!")
