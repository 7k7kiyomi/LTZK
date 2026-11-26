from __future__ import annotations

import argparse
import struct
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont


TITLE = "陇图智控平台"
EXPECTED_SIZE = (1977, 946)


def read_psd_composite(psd_path: Path) -> Image.Image:
    with psd_path.open("rb") as f:
        header = f.read(26)
        if header[:4] != b"8BPS":
            raise ValueError(f"{psd_path} is not a PSD file")

        channels = struct.unpack(">H", header[12:14])[0]
        height = struct.unpack(">I", header[14:18])[0]
        width = struct.unpack(">I", header[18:22])[0]
        depth = struct.unpack(">H", header[22:24])[0]
        color_mode = struct.unpack(">H", header[24:26])[0]

        if (width, height) != EXPECTED_SIZE:
            raise ValueError(f"PSD size is {width}x{height}, expected {EXPECTED_SIZE[0]}x{EXPECTED_SIZE[1]}")
        if channels < 3 or depth != 32 or color_mode != 3:
            raise ValueError(f"Unsupported PSD composite: channels={channels}, depth={depth}, color_mode={color_mode}")

        color_data_len = struct.unpack(">I", f.read(4))[0]
        f.seek(color_data_len, 1)
        image_resources_len = struct.unpack(">I", f.read(4))[0]
        f.seek(image_resources_len, 1)
        layer_mask_len = struct.unpack(">I", f.read(4))[0]
        f.seek(layer_mask_len, 1)

        compression = struct.unpack(">H", f.read(2))[0]
        if compression != 0:
            raise ValueError(f"Unsupported PSD composite compression: {compression}")

        byte_count = width * height * channels * (depth // 8)
        data = f.read(byte_count)
        if len(data) != byte_count:
            raise ValueError("PSD composite image data is truncated")

    planes = np.frombuffer(data, dtype=">f4").reshape(channels, height, width)
    rgb = np.moveaxis(planes[:3], 0, 2)
    rgb = np.clip(rgb, 0.0, 1.0)
    image = (rgb * 255.0 + 0.5).astype(np.uint8)
    return Image.fromarray(image, "RGB")


def remove_existing_title(base: Image.Image) -> Image.Image:
    image = np.array(base)
    mask = cv2.inRange(image, np.array([235, 235, 235], dtype=np.uint8), np.array([255, 255, 255], dtype=np.uint8))

    # Limit the edit to the central title band so small bright map details remain intact.
    title_band = np.zeros(mask.shape, dtype=np.uint8)
    title_band[310:625, 120:1860] = 255
    mask = cv2.bitwise_and(mask, title_band)

    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (31, 31))
    mask = cv2.dilate(mask, kernel, iterations=2)
    repaired = cv2.inpaint(image, mask, 9, cv2.INPAINT_TELEA)
    return Image.fromarray(repaired, "RGB")


def add_title_backplate(background: Image.Image) -> Image.Image:
    base = np.array(background).astype(np.float32)
    height, width = base.shape[:2]

    x = np.arange(width, dtype=np.float32)
    y = np.arange(height, dtype=np.float32)
    xx, yy = np.meshgrid(x, y)

    horizontal = np.clip((xx - 110) / 170, 0, 1) * np.clip((width - 110 - xx) / 170, 0, 1)
    vertical = np.clip((yy - 305) / 85, 0, 1) * np.clip((645 - yy) / 85, 0, 1)
    alpha = (horizontal * vertical * 0.94)[:, :, None]

    plate_color = np.array([0, 57, 205], dtype=np.float32)
    out = base * (1.0 - alpha) + plate_color * alpha

    glow = np.exp(-(((xx - width / 2) / 700) ** 2 + ((yy - 470) / 170) ** 2))[:, :, None]
    out = out * (1.0 - glow * 0.12) + np.array([0, 105, 255], dtype=np.float32) * (glow * 0.12)

    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def draw_new_title(background: Image.Image, font_path: Path) -> Image.Image:
    image = background.copy()
    draw = ImageDraw.Draw(image)

    font_size = 235
    font = ImageFont.truetype(str(font_path), font_size)
    bbox = draw.textbbox((0, 0), TITLE, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]

    x = (EXPECTED_SIZE[0] - text_width) // 2 - bbox[0]
    y = 472 - text_height // 2 - bbox[1]
    draw.text((x, y), TITLE, fill=(255, 255, 255), font=font)
    return image


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate LTZK splash PNG from the provided PSD composite.")
    parser.add_argument("--psd", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--base-preview", type=Path)
    parser.add_argument("--repaired-preview", type=Path)
    parser.add_argument("--font", default=Path(r"C:\Windows\Fonts\simhei.ttf"), type=Path)
    args = parser.parse_args()

    base = read_psd_composite(args.psd)
    if args.base_preview:
        args.base_preview.parent.mkdir(parents=True, exist_ok=True)
        base.save(args.base_preview)

    repaired = add_title_backplate(remove_existing_title(base))
    if args.repaired_preview:
        args.repaired_preview.parent.mkdir(parents=True, exist_ok=True)
        repaired.save(args.repaired_preview)

    splash = draw_new_title(repaired, args.font)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    splash.save(args.out)


if __name__ == "__main__":
    main()
