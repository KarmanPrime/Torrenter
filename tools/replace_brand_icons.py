"""Rasterize the supplied Torrenter brand SVGs and replace legacy qBittorrent brand assets.

Uses a tiny Qt QSvgRenderer helper for pixel-accurate SVG rasterization, then
Pillow for multi-size ICO/ICNS containers. Source SVGs are never modified.
"""
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

RENDERER = Path(__file__).with_name("brand_svg_render.exe")
BRAND = Path(r"C:\Users\Scott\Documents\Grok Bot\Benji\Torrenter\torrenter-design\brand")
REPO = Path(r"C:\Users\Scott\Desktop\Torrenter\torrenter")
GEN = REPO / "tools" / "brand-generated"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
MENU_SIZES = (16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 192)


def render(svg: Path, out: Path, size: int) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(RENDERER), str(svg), str(out), str(size)], check=True)


def glyph_svg(color: str) -> str:
    src = (BRAND / "torrenter-glyph.svg").read_text(encoding="utf-8")
    return src.replace("currentColor", color)


def make_ico(src_pngs, out: Path) -> None:
    images = [Image.open(p).convert("RGBA") for p in src_pngs]
    largest = max(images, key=lambda im: im.width)
    sizes = sorted({im.width for im in images})
    largest.save(out, format="ICO", sizes=[(s, s) for s in sizes])


def make_icns(src_pngs, out: Path) -> None:
    entries = []
    for size, code in ((128, b"ic07"), (256, b"ic08"), (512, b"ic09")):
        p = next((p for p in src_pngs if Image.open(p).width == size), None)
        if p is None:
            continue
        data = p.read_bytes()
        entries.append(struct.pack(">I", len(data) + 8) + code + data)
    body = b"".join(entries)
    out.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)


def replace_svg(dst: Path, content: str) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(content, encoding="utf-8")


def main() -> int:
    GEN.mkdir(parents=True, exist_ok=True)
    icon_svg = BRAND / "torrenter-icon.svg"
    light_svg = BRAND / "torrenter-icon-light.svg"
    mark_svg = BRAND / "torrenter-mark.svg"

    dark_tray_svg = GEN / "torrenter-glyph-dark.svg"
    light_tray_svg = GEN / "torrenter-glyph-light.svg"
    accent_tray_svg = GEN / "torrenter-glyph-accent.svg"
    dark_tray_svg.write_text(glyph_svg("#FAFAFA"), encoding="utf-8")
    light_tray_svg.write_text(glyph_svg("#3C3C3C"), encoding="utf-8")
    accent_tray_svg.write_text(glyph_svg("#2FC99F"), encoding="utf-8")

    icon_pngs = []
    for size in SIZES:
        out = GEN / f"torrenter-icon-{size}.png"
        render(icon_svg, out, size)
        icon_pngs.append(out)
        render(light_svg, GEN / f"torrenter-icon-light-{size}.png", size)
        render(mark_svg, GEN / f"torrenter-mark-{size}.png", size)
        render(dark_tray_svg, GEN / f"torrenter-tray-dark-{size}.png", size)
        render(light_tray_svg, GEN / f"torrenter-tray-light-{size}.png", size)
        render(accent_tray_svg, GEN / f"torrenter-tray-accent-{size}.png", size)

    # Windows resources: keep names, swap contents.
    make_ico([GEN / f"torrenter-icon-{s}.png" for s in SIZES], REPO / "src/icons/qbittorrent.ico")
    make_ico([GEN / f"torrenter-mark-{s}.png" for s in SIZES], REPO / "src/icons/qbittorrent_file.ico")
    make_icns([GEN / f"torrenter-icon-{s}.png" for s in SIZES], REPO / "dist/mac/qbittorrent_mac.icns")
    make_icns([GEN / f"torrenter-mark-{s}.png" for s in SIZES], REPO / "dist/mac/qBitTorrentDocument.icns")
    make_icns([GEN / f"torrenter-tray-accent-{s}.png" for s in SIZES], REPO / "dist/mac/qBitTorrentIncompleteData.icns")

    # SVG assets: keep names/aliases, swap contents.
    replace_svg(REPO / "src/icons/qbittorrent-tray.svg", icon_svg.read_text(encoding="utf-8"))
    replace_svg(REPO / "src/icons/qbittorrent_mac.svg", icon_svg.read_text(encoding="utf-8"))
    replace_svg(REPO / "src/icons/qbittorrent-tray-dark.svg", dark_tray_svg.read_text(encoding="utf-8"))
    replace_svg(REPO / "src/icons/qbittorrent-tray-light.svg", light_tray_svg.read_text(encoding="utf-8"))
    replace_svg(REPO / "dist/unix/menuicons/scalable/apps/qbittorrent.svg", icon_svg.read_text(encoding="utf-8"))
    replace_svg(REPO / "src/webui/www/public/images/qbittorrent-tray.svg", accent_tray_svg.read_text(encoding="utf-8"))

    # Unix menu icons.
    for size in MENU_SIZES:
        src_icon = GEN / f"torrenter-icon-{size}.png"
        if not src_icon.exists():
            render(icon_svg, src_icon, size)
        src_tray = GEN / f"torrenter-tray-accent-{size}.png"
        if not src_tray.exists():
            render(accent_tray_svg, src_tray, size)
        dst_icon = REPO / f"dist/unix/menuicons/{size}x{size}/apps/qbittorrent.png"
        dst_tray = REPO / f"dist/unix/menuicons/{size}x{size}/status/qbittorrent-tray.png"
        shutil.copyfile(src_icon, dst_icon)
        shutil.copyfile(src_tray, dst_tray)

    # WebUI 32 px asset.
    shutil.copyfile(GEN / "torrenter-icon-32.png", REPO / "src/webui/www/public/images/qbittorrent32.png")

    # Splash: dark brand tile with the same 350x300 footprint.
    splash = Image.new("RGB", (350, 300), (12, 13, 15))
    draw = ImageDraw.Draw(splash)
    for y in range(300):
        t = y / 299
        draw.line((0, y, 350, y), fill=(int(27 - 15 * t), int(29 - 16 * t), int(33 - 18 * t)))
    tile = Image.open(GEN / "torrenter-icon-128.png").convert("RGBA").resize((104, 104), Image.Resampling.LANCZOS)
    splash.paste(tile, (24, 46), tile)
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf", 30)
    except OSError:
        font = ImageFont.load_default()
    draw.text((24, 178), "Torrenter", font=font, fill=(122, 240, 208))
    draw.text((24, 216), "BitTorrent client", font=font, fill=(226, 230, 228))
    splash.save(REPO / "src/icons/splash.png")

    print("brand assets replaced")
    return 0


def qbittorrent_size(size: int) -> int:
    return size


if __name__ == "__main__":
    sys.exit(main())
