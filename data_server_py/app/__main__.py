"""本地启动入口：python -m app

默认 0.0.0.0:9000（与 C++ 版一致）；本地调试可用环境变量换端口：
    CCMEETING_AUTH_PORT=9100 python -m app
"""

from __future__ import annotations

import logging
import sys

import uvicorn

from .config import load_config


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        stream=sys.stdout,
    )
    config = load_config()
    print(f"[auth-py] sqlite : {config.sqlite_file()}")
    print(f"[auth-py] static : {config.static_dir()}")
    print(f"[auth-py] uploads: {config.upload_dir()}")
    print(f"[auth-py] base   : {config.assets.public_base_url}")
    print(f"[auth-py] listen : http://{config.listen_address}:{config.listen_port}")
    uvicorn.run(
        # 用工厂函数，避免 import 阶段就建数据库（测试里也复用 create_app）
        "app.api:create_app",
        factory=True,
        host=config.listen_address,
        port=config.listen_port,
        log_level="info",
        # 由中间件统一写 Server: CloudMeetingAuthServer，禁掉 uvicorn 自己的
        # 单进程即可满足当前规模；多进程会各自持有 sqlite 连接反而无益
        server_header=False,
        workers=1,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
