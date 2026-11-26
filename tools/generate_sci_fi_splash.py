from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


SIZE = (1977, 946)
TITLE = "\u9647\u56fe\u667a\u63a7\u5e73\u53f0"
SUBTITLE = "\u2014\u2014 \u7518\u8083\u7701\u57fa\u7840\u5730\u7406\u4fe1\u606f\u4e2d\u5fc3 \u2014\u2014"


def lerp(a: int, b: int, t: float) -> int:
    return int(a + (b - a) * t + 0.5)


def mix(c1: tuple[int, int, int], c2: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    candidates = [
        Path(r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
        Path(r"C:\Windows\Fonts\simsun.ttc"),
    ]
    for font in candidates:
        if font.exists():
            return ImageFont.truetype(str(font), size)
    return ImageFont.load_default()


def draw_background(draw: ImageDraw.ImageDraw, width: int, height: int) -> None:
    top = (12, 31, 50)
    middle = (16, 43, 70)
    bottom = (10, 26, 45)
    for y in range(height):
        t = y / max(1, height - 1)
        if t < 0.54:
            color = mix(top, middle, t / 0.54)
        else:
            color = mix(middle, bottom, (t - 0.54) / 0.46)
        draw.line((0, y, width, y), fill=color)


def add_glow_layer(base: Image.Image) -> None:
    width, height = base.size
    glow = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(glow)
    draw.ellipse((-170, -250, 760, 620), fill=(27, 201, 255, 32))
    draw.ellipse((width - 780, -210, width + 210, 650), fill=(255, 178, 56, 20))
    draw.ellipse((width // 2 - 650, 250, width // 2 + 650, 930), fill=(27, 201, 255, 30))
    base.alpha_composite(glow.filter(ImageFilter.GaussianBlur(85)))


def add_atmospheric_sci_fi_layer(base: Image.Image) -> None:
    width, height = base.size
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)

    draw.polygon(
        [(-120, 210), (width * 0.38, 120), (width + 120, 410), (width + 120, 510), (width * 0.32, 250), (-120, 340)],
        fill=(27, 201, 255, 22),
    )
    draw.polygon(
        [(-160, height - 220), (width * 0.42, height - 330), (width + 160, height - 150), (width + 160, height - 70), (width * 0.44, height - 230), (-160, height - 110)],
        fill=(255, 178, 56, 14),
    )
    draw.ellipse((width * 0.18, height * 0.18, width * 0.82, height * 1.18), fill=(27, 201, 255, 16))

    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(72)))

def add_gis_spatial_depth_layer(base: Image.Image) -> None:
    width, height = base.size
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)

    scan_arcs = (
        (width // 2 - 650, 118, width // 2 + 650, 820, 198, 338, (27, 201, 255, 38), 3),
        (width // 2 - 510, 188, width // 2 + 510, 760, 207, 326, (204, 237, 255, 24), 2),
        (width // 2 - 760, 80, width // 2 + 760, 900, 212, 318, (255, 178, 56, 18), 2),
    )
    for left, top, right, bottom, start, end, color, line_width in scan_arcs:
        draw.arc((left, top, right, bottom), start=start, end=end, fill=color, width=line_width)

    map_paths = (
        [(210, 390), (390, 335), (560, 370), (720, 315), (890, 340)],
        [(1088, 348), (1248, 304), (1415, 336), (1590, 286), (1770, 330)],
        [(258, 594), (430, 548), (610, 590), (790, 532), (948, 558)],
        [(1040, 574), (1215, 530), (1390, 568), (1575, 515), (1755, 552)],
    )
    for path in map_paths:
        draw.line(path, fill=(27, 201, 255, 36), width=2, joint="curve")

    coordinate_points = (
        (210, 390), (560, 370), (890, 340), (1248, 304), (1590, 286),
        (258, 594), (610, 590), (948, 558), (1390, 568), (1755, 552),
    )
    for x, y in coordinate_points:
        draw.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(204, 237, 255, 96))
        draw.ellipse((x - 12, y - 12, x + 12, y + 12), fill=(27, 201, 255, 18))

    telemetry_surfaces = (
        (92, 282, 314, 334),
        (132, 356, 405, 406),
        (1570, 292, 1848, 344),
        (1640, 376, 1885, 426),
    )
    for surface in telemetry_surfaces:
        draw.rounded_rectangle(surface, radius=6, fill=(6, 24, 40, 42))
        x1, y1, x2, y2 = surface
        draw.line((x1 + 18, y1 + 16, x2 - 18, y1 + 16), fill=(204, 237, 255, 36), width=1)
        draw.line((x1 + 18, y2 - 14, x2 - 78, y2 - 14), fill=(255, 178, 56, 24), width=1)

    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(0.6)))
    base.alpha_composite(layer)

def add_circuit_sci_fi_layer(base: Image.Image) -> None:
    width, height = base.size
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)

    circuit_segments = (
        (150, 145, 330, 145),
        (330, 145, 330, 210),
        (330, 210, 470, 210),
        (118, 725, 300, 725),
        (300, 725, 300, 660),
        (300, 660, 455, 660),
        (width - 470, 180, width - 330, 180),
        (width - 330, 180, width - 330, 250),
        (width - 330, 250, width - 145, 250),
        (width - 455, 690, width - 295, 690),
        (width - 295, 690, width - 295, 755),
        (width - 295, 755, width - 110, 755),
    )
    for x1, y1, x2, y2 in circuit_segments:
        draw.line((x1, y1, x2, y2), fill=(27, 201, 255, 30), width=2)

    for x, y in ((150, 145), (330, 210), (455, 660), (118, 725)):
        draw.rectangle((x - 18, y - 5, x + 18, y + 5), fill=(27, 201, 255, 34))

    for x, y in ((150, 250), (330, 180), (455, 690), (110, 755)):
        draw.rectangle((width - x - 18, y - 5, width - x + 18, y + 5), fill=(255, 178, 56, 20))

    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(1.2)))
    base.alpha_composite(layer)

def draw_text(base: Image.Image) -> None:
    width, _ = base.size
    title_font = load_font(235, bold=False)
    subtitle_font = load_font(65, bold=False)
    draw = ImageDraw.Draw(base)

    title_bbox = draw.textbbox((0, 0), TITLE, font=title_font)
    title_w = title_bbox[2] - title_bbox[0]
    title_h = title_bbox[3] - title_bbox[1]
    title_x = (width - title_w) // 2 - title_bbox[0]
    title_y = 448 - title_h // 2 - title_bbox[1]

    subtitle_bbox = draw.textbbox((0, 0), SUBTITLE, font=subtitle_font)
    subtitle_w = subtitle_bbox[2] - subtitle_bbox[0]
    subtitle_x = (width - subtitle_w) // 2 - subtitle_bbox[0]
    subtitle_y = title_y + title_h + 82 - subtitle_bbox[1]

    glow = Image.new("RGBA", base.size, (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow)
    glow_draw.text((title_x, title_y), TITLE, font=title_font, fill=(27, 201, 255, 150))
    glow_draw.text((subtitle_x, subtitle_y), SUBTITLE, font=subtitle_font, fill=(27, 201, 255, 115))
    base.alpha_composite(glow.filter(ImageFilter.GaussianBlur(10)))
    draw.text((title_x + 5, title_y + 7), TITLE, font=title_font, fill=(3, 12, 22, 125))
    draw.text((title_x, title_y), TITLE, font=title_font, fill=(255, 255, 255, 255))
    draw.text((subtitle_x, subtitle_y), SUBTITLE, font=subtitle_font, fill=(204, 237, 255, 235))



def build_splash() -> Image.Image:
    image = Image.new("RGBA", SIZE, (0, 0, 0, 255))
    draw_background(ImageDraw.Draw(image), *SIZE)
    add_glow_layer(image)
    add_atmospheric_sci_fi_layer(image)
    add_gis_spatial_depth_layer(image)
    add_circuit_sci_fi_layer(image)
    draw_text(image)
    return image.convert("RGB")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate LTZK sci-fi themed splash PNG.")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    args.out.parent.mkdir(parents=True, exist_ok=True)
    build_splash().save(args.out)


if __name__ == "__main__":
    main()
