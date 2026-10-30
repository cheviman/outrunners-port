"""
Name:        make_icon.py
Description: draws the project's icon: a steering wheel, the cabinet's controller, with speed lines on a plate
             in a sunset's colours; nothing of the game's
Version:     0.1.0
Created:     30-10-2026
Modified:    30-10-2026
Author:      Sebastian Toyos

Usage: python tools/make_icon.py

As SegaSonic's (its trackball, here the wheel): the icon of the Switch's build (ports/switch/icon.jpg,
256 x 256: the plate to the edges, which the HOME menu rounds itself), of the Android app
(ports/android/res/mipmap-xxxhdpi: an adaptive icon's two layers, the plate to the edges and the wheel in the
middle 66 of its 108, which the launcher shapes and moves itself; the icon whole for the launchers before
Android 8; and the picture the APK shows in a player's own picture's place until one is put there,
ic_launcher_art.png) and of the computers' executables (ports/sdl2/icon: outrunners.png, 1024 x 1024,
outrunners.ico and outrunners.icns). Drawn at twice the size and scaled down, so its edges are smooth.
"""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "ports" / "sdl2" / "icon"
SWITCH = ROOT / "ports" / "switch" / "icon.jpg"
ANDROID = ROOT / "ports" / "android" / "res" / "mipmap-xxxhdpi"
ANDROID_ICON = 192              # (the launcher's icon at xxxhdpi: 48 dp)
ANDROID_LAYER = 432             # (an adaptive icon's layer at xxxhdpi: 108 dp)
ANDROID_SAFE = 66 / 108         # (the part of a layer every launcher's shape shows)
ANDROID_ART = 288               # (a picture in what the launcher shows of a layer at xxxhdpi: 72 dp)
SIZE = 1024
SCALE = 2                       # (drawn at 2048, then scaled down)
ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]

PLATE_TOP = np.array([0x24, 0x1A, 0x6E])     # the plate: a sunset, deep blue at the top to orange
PLATE_MIDDLE = np.array([0xC8, 0x2E, 0x6A])
PLATE_BOTTOM = np.array([0xFF, 0x8C, 0x2E])
RIM_LIGHT = np.array([0x6A, 0x70, 0x80])     # the rim: black, lit from the top left
RIM_DARK = np.array([0x0C, 0x0D, 0x12])
SPOKE = np.array([0xC4, 0xCA, 0xD6])         # the spokes: steel
HUB = np.array([0x16, 0x18, 0x20])
BADGE = np.array([0xE2, 0x22, 0x2A])         # the hub's badge: red
LINE = np.array([0xFF, 0xF2, 0xC0])          # the speed lines: pale yellow


def coverage(distance, pixel):
    """How much of a pixel a shape covers, from its signed distance to the edge (negative inside)."""
    return np.clip(0.5 - distance / pixel, 0.0, 1.0)


def rounded_square(x, y, left, top, right, bottom, radius):
    """Signed distance to a square with rounded corners."""
    cx, cy = (left + right) / 2, (top + bottom) / 2
    qx = np.abs(x - cx) - ((right - left) / 2 - radius)
    qy = np.abs(y - cy) - ((bottom - top) / 2 - radius)
    outside = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0))
    return outside + np.minimum(np.maximum(qx, qy), 0) - radius


def segment(x, y, x0, y0, x1, y1, radius):
    """Signed distance to a bar with round ends from (x0, y0) to (x1, y1)."""
    dx, dy = x1 - x0, y1 - y0
    along = np.clip(((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy), 0, 1)
    return np.hypot(x - x0 - along * dx, y - y0 - along * dy) - radius


def over(image, color, alpha):
    """Paints 'color' over 'image' (RGBA, floats) where 'alpha' says."""
    alpha = alpha[..., None]
    image[..., :3] = image[..., :3] * (1 - alpha) + color * alpha
    image[..., 3:] = image[..., 3:] * (1 - alpha) + alpha


def to_image(image):
    image = Image.fromarray(np.clip(image * [1, 1, 1, 255], 0, 255).round().astype(np.uint8), "RGBA")
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def draw(margin=48, radius=210, plate_shown=True, wheel_shown=True, wheel_scale=1.0):
    """The icon; 'margin' and 'radius' are the plate's room around it and its corners' (0: to the edges).
    Without the plate or without the wheel (its lines and the wheel), the other alone on nothing (an
    adaptive icon's layers), the wheel 'wheel_scale' times its size about the middle."""
    size = SIZE * SCALE
    unit = size / SIZE
    y, x = np.mgrid[0:size, 0:size].astype(np.float64) + 0.5
    x, y = x / unit, y / unit           # (in the 1024 icon's pixels)
    pixel = 1.0 / unit
    image = np.zeros((size, size, 4))

    # the plate: the sky from the top down
    plate = coverage(rounded_square(x, y, margin, margin, SIZE - margin, SIZE - margin, max(radius, 1)), pixel)
    shade = np.clip((y - margin) / (SIZE - 2 * margin), 0, 1)[..., None]
    upper = PLATE_TOP * (1 - shade * 2) + PLATE_MIDDLE * (shade * 2)
    lower = PLATE_MIDDLE * (2 - shade * 2) + PLATE_BOTTOM * (shade * 2 - 1)
    color = np.where(shade < 0.5, upper, lower)
    if plate_shown:
        over(image, color, plate)
    else:
        plate = np.ones_like(plate)     # (the lines on nothing: whole)
    if not wheel_shown:
        return to_image(image)
    x = (x - SIZE / 2) / wheel_scale + SIZE / 2    # (the wheel's own pixels, about the middle)
    y = (y - SIZE / 2) / wheel_scale + SIZE / 2
    pixel = pixel / wheel_scale
    cx, cy = 590.0, 540.0

    # the speed lines, fading to the left
    for line_y, start, half in ((330, 150, 22), (470, 96, 22), (610, 120, 22), (750, 180, 22)):
        end = cx - 230
        alpha = coverage(segment(x, y, start, line_y, end, line_y, half), pixel)
        fade = np.clip((x - start + half) / (end - start + half), 0, 1) ** 0.6
        over(image, LINE, alpha * fade * plate)

    # the wheel: a rim lit from the top left, three steel spokes (left, right, down) and the hub with its badge
    distance = np.hypot(x - cx, y - cy)
    toward = ((x - cx) * -0.6 + (y - cy) * -0.8) / np.maximum(distance, 1)     # 1 facing the light
    for start, end, half in ((cx - 110, cx - 270, 34), (cx + 110, cx + 270, 34)):
        over(image, SPOKE, coverage(segment(x, y, start, cy + 10, end, cy + 10, half), pixel))
    over(image, SPOKE, coverage(segment(x, y, cx, cy + 100, cx, cy + 270, 40), pixel))
    lit = np.clip((toward + 1) / 2, 0, 1)[..., None] ** 1.5
    rim = np.abs(distance - 270) - 42
    over(image, RIM_DARK * (1 - lit) + RIM_LIGHT * lit, coverage(rim, pixel))
    sheen = np.clip(1 - np.abs(distance - 285) / 14, 0, 1) * np.clip(toward, 0, 1) ** 3
    over(image, np.array([0xE8, 0xEC, 0xF4]), sheen * 0.6 * coverage(rim, pixel))    # (the rim's shine)
    over(image, HUB, coverage(distance - 118, pixel))
    over(image, SPOKE, coverage(np.abs(distance - 112) - 6, pixel))
    over(image, BADGE, coverage(distance - 58, pixel))
    over(image, np.array([0xFF, 0x8A, 0x8A]), coverage(np.hypot(x - cx + 18, y - cy + 20) - 16, pixel) * 0.8)

    return to_image(image)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    icon = draw()
    icon.save(OUT / "outrunners.png")
    icon.save(OUT / "outrunners.ico", sizes=[(side, side) for side in ICO_SIZES])
    icon.save(OUT / "outrunners.icns")
    print(f"written: outrunners.png, outrunners.ico, outrunners.icns in {OUT.relative_to(ROOT)}")
    SWITCH.parent.mkdir(parents=True, exist_ok=True)
    draw(margin=0, radius=0).convert("RGB").resize((256, 256), Image.LANCZOS).save(SWITCH, quality=95)
    print(f"written: {SWITCH.relative_to(ROOT)}")
    ANDROID.mkdir(parents=True, exist_ok=True)
    icon.resize((ANDROID_ICON, ANDROID_ICON), Image.LANCZOS).save(ANDROID / "ic_launcher.png")
    background = draw(margin=0, radius=0, wheel_shown=False).convert("RGB")
    background.resize((ANDROID_LAYER, ANDROID_LAYER), Image.LANCZOS).save(ANDROID / "ic_launcher_background.png")
    foreground = draw(plate_shown=False, wheel_scale=ANDROID_SAFE)
    foreground.resize((ANDROID_LAYER, ANDROID_LAYER), Image.LANCZOS).save(ANDROID / "ic_launcher_foreground.png")
    art = draw(margin=0, radius=0).convert("RGB")
    art.resize((ANDROID_ART, ANDROID_ART), Image.LANCZOS).save(ANDROID / "ic_launcher_art.png")
    print(f"written: ic_launcher.png, ic_launcher_background.png, ic_launcher_foreground.png, ic_launcher_art.png "
          f"in {ANDROID.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
