"""UI 截图回归：对比两份截图目录，报告每张图的差异像素比例与差异区域。

用途：主题化 / 重构之后，确认"除了预期的改动区域，其余像素一致"。

用法：
    python image_diff.py <baseline_dir> <current_dir> [--expect-sidebar]

约定：
- 只比较两边都存在的同名 png；尺寸不同直接判为"结构变化"
- 输出每张图的 差异像素 / 总像素 / 百分比 / 差异包围盒
- --expect-sidebar：额外判定"差异是否全部落在左侧 240px 内"（侧栏改动）
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageChops


def compare(baseline: Path, current: Path, expect_sidebar: bool) -> int:
    names = sorted(p.name for p in baseline.glob("*.png") if (current / p.name).exists())
    if not names:
        print("没有可对比的同名截图")
        return 1

    print(f"{'截图':<28}{'尺寸':<14}{'差异像素':>10}{'占比':>9}  差异区域")
    print("-" * 86)

    failures = 0
    for name in names:
        a = Image.open(baseline / name).convert("RGB")
        b = Image.open(current / name).convert("RGB")
        if a.size != b.size:
            print(f"{name:<28}{str(a.size):<14}{'—':>10}{'—':>9}  尺寸变化 -> {b.size}")
            failures += 1
            continue

        diff = ImageChops.difference(a, b)
        bbox = diff.getbbox()
        total = a.size[0] * a.size[1]
        if bbox is None:
            print(f"{name:<28}{str(a.size):<14}{0:>10}{0.0:>8.2f}%  完全一致")
            continue

        # 统计非零像素
        gray = diff.convert("L")
        hist = gray.histogram()
        changed = total - hist[0]
        ratio = changed / total * 100
        region = f"x={bbox[0]}..{bbox[2]} y={bbox[1]}..{bbox[3]}"

        note = ""
        if expect_sidebar:
            if bbox[2] <= 240:
                note = "  (侧栏内，预期)"
            else:
                note = "  !! 超出侧栏，需检查"
                failures += 1
        print(f"{name:<28}{str(a.size):<14}{changed:>10}{ratio:>8.2f}%  {region}{note}")

    print("-" * 86)
    print(f"对比 {len(names)} 张；不符合预期 {failures} 张")
    return 0 if failures == 0 else 1


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    baseline = Path(sys.argv[1])
    current = Path(sys.argv[2])
    expect_sidebar = "--expect-sidebar" in sys.argv[3:]
    return compare(baseline, current, expect_sidebar)


if __name__ == "__main__":
    raise SystemExit(main())
