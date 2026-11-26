"""从 SVG 母版生成整套图标资源（PNG 多尺寸 + Windows .ico）。

- 大尺寸（>=32）用 logo.svg（细笔画）
- 小尺寸（<=24）用 logo-small.svg（加粗笔画，避免 16px 糊成一团）
- 输出：design/logo/out/*.png、design/logo/app.ico、client/resources/images/logo.svg
"""

from __future__ import annotations

import os
import subprocess
import sys

from PIL import Image

EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(ROOT, "..", ".."))
OUT = os.path.join(ROOT, "out")
QRC_IMG_DIR = os.path.join(REPO, "client", "resources", "images")

SIZES = [16, 24, 32, 48, 64, 128, 256, 512]


def render_svg(svg_path: str, png_path: str, size: int = 512):
    url = "file:///" + svg_path.replace("\\", "/")
    if os.path.exists(png_path):
        os.remove(png_path)
    subprocess.run(
        [
            EDGE,
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            "--force-device-scale-factor=1",
            "--default-background-color=00000000",
            f"--window-size={size},{size}",
            f"--screenshot={png_path}",
            url,
        ],
        capture_output=True,
        timeout=120,
    )
    if not os.path.exists(png_path):
        return None
    img = Image.open(png_path).convert("RGBA")
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    return img.resize((size, size), Image.LANCZOS)


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    big = render_svg(os.path.join(ROOT, "logo.svg"), os.path.join(OUT, "_big.png"))
    small = render_svg(os.path.join(ROOT, "logo-small.svg"), os.path.join(OUT, "_small.png"))
    if big is None or small is None:
        print("渲染失败：Edge headless 没产出 PNG")
        return 1

    for size in SIZES:
        src = small if size <= 24 else big
        src.resize((size, size), Image.LANCZOS).save(
            os.path.join(OUT, f"logo-{size}.png")
        )
    print("png:", ", ".join(f"logo-{s}.png" for s in SIZES))

    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    frames = [small.resize((16, 16), Image.LANCZOS), small.resize((24, 24), Image.LANCZOS)]
    frames += [big.resize((s, s), Image.LANCZOS) for s, _ in ico_sizes[2:]]
    ico_path = os.path.join(ROOT, "app.ico")
    frames[-1].save(ico_path, format="ICO", sizes=ico_sizes, append_images=frames[:-1])
    print("ico:", ico_path)

    svg_dst = os.path.join(QRC_IMG_DIR, "logo.svg")
    os.makedirs(QRC_IMG_DIR, exist_ok=True)
    with open(os.path.join(ROOT, "logo.svg"), "r", encoding="utf-8") as fh:
        content = fh.read()
    with open(svg_dst, "w", encoding="utf-8") as fh:
        fh.write(content)
    print("svg:", svg_dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())