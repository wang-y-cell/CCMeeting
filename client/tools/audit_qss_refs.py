"""QSS 引用审计：确保代码里写的 :/Style/source/*.qss 全部真实存在。

背景：一次脚本化重命名把 `meeting_settings_popup.qss` 改成了
`MeetingSettingsPopup.qss`，导致两个窗口静默失去样式（主题"消失"）。
这个脚本把这类拼写错误变成可自动检查的问题。

用法：
    python client/tools/audit_qss_refs.py
退出码：0 = 全部存在；1 = 有缺失引用
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

CLIENT = Path(__file__).resolve().parent.parent
THEME = CLIENT / "src" / "ui" / "theme"
PATTERN = re.compile(r":/Style/source/([A-Za-z0-9_.\-]+\.qss)")


def main() -> int:
    existing = {p.name for p in THEME.glob("*.qss")}
    if not existing:
        print(f"[audit] 主题目录为空: {THEME}")
        return 1

    problems: list[str] = []
    checked = 0
    for base in ("src", "tools", "include"):
        for path in (CLIENT / base).rglob("*"):
            if path.suffix not in {".cpp", ".h"}:
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue
            for name in PATTERN.findall(text):
                if "xxx" in name.lower():  # 文档里的占位符
                    continue
                checked += 1
                if name not in existing:
                    problems.append(f"{path.relative_to(CLIENT)}: {name}")

    print(f"[audit] 引用 {checked} 处，主题目录有 {len(existing)} 个 qss")
    for item in problems:
        print(f"[audit] 缺失 -> {item}")
    print(f"[audit] 结论: {'PASS' if not problems else 'FAIL'}")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
