# message_server：C++ vs Python 压测结果

测试环境：同一台开发机（C++ 服务端跑在 WSL2 Ubuntu 24.04，Python 服务端跑在同一 WSL2，
压测客户端跑在 Windows，经 localhost 转发），同机对比，避免公网 RTT 干扰。

- C++：`message_server` 源码（Release，`-O2`，Boost.Beast + nlohmann/json）
- Python：`bench/py_server.py`（asyncio + websockets 10.4 + **标准库 json**，未用 uvloop/orjson）
- 客户端：`bench/bench_client.py`（asyncio，单进程）

## 一、CPU 成本（最关键）

| 场景 | C++ | Python | 倍数 |
|---|---|---|---|
| 50 客户端 × 20 条（49,000 次投递，同一配置重复 3 次中位数） | **150 ms** | **370 ms** | **2.5×** |
| 100 客户端 × 10 条（99,000 次投递） | 250 ms | 640 ms | 2.6× |
| 微基准 A：2 客户端 × 3000 条（扇出 1，每千次投递 CPU） | **5.0 ms** | **13.3 ms** | 2.7× |
| 微基准 B：1 客户端 × 3000 条（无扇出，每千条解析/分发） | **3.3 ms** | **10.0 ms** | 3.0× |

按单核推算容量：

| 指标 | C++ | Python |
|---|---|---|
| 纯解析+分发（无扇出） | ≈ 300k msg/s | ≈ 100k msg/s |
| 每次投递（扇出 1） | ≈ 200k 投递/s | ≈ 75k 投递/s |

## 二、内存

| 指标 | C++ | Python |
|---|---|---|
| 空闲 RSS | 1.6 MB | 7.1 MB |
| 200 连接时 RSS | 2.0 MB | 8.0 MB |
| 每连接增量 | ≈ **1.9 KB** | ≈ **4.6 KB** |

## 三、延迟（端到端，含压测客户端自身开销）

| 配置 | C++ text p50 / p99 | Python text p50 / p99 | C++ join p50 | Python join p50 |
|---|---|---|---|---|
| 50 客户端 × 20 条 | 8.5 / 18.5 ms | 15.3 / 22.6 ms | 15.0 ms | 9.1 ms |
| 100 客户端 × 10 条 | 53~60 / 105~166 ms | 78~80 / 138~155 ms | 29.3 ms | 34.1 ms |

> 注意：这些延迟**包含了压测客户端自己处理 49k~99k 条广播的时间**。单进程客户端是瓶颈
> （两个服务端的端到端投递率都约 82k/s，几乎相同），所以延迟数字是"上限"，不能直接
> 当作服务端延迟。服务端真实成本看上表的 CPU 数据更准确。

## 四、结论

1. **纯效率：C++ 约快 2.5~3 倍**（每消息 CPU），内存约省 2.5 倍（每连接）。
2. **Python 的性能余量仍然很大**：按你当前规模（≤100 人/场、每场每秒几条消息），
   峰值投递量约 500~1000 次/s，Python 单核即可承载约 75k/s → 占用 < 2% CPU。
3. **延迟差异在实际使用中不可感**：两者在 50 人场景的端到端 p50 都在 8~15ms 量级，
   远小于网络 RTT（公网 20~60ms）与 Janus 媒体处理时间。
4. **Python 还有优化空间**：本次未装 uvloop / orjson；实测经验是两者叠加可再降
   30%~50% CPU（预计 13.3 → 7~9 ms/千次投递）。
5. **不建议为了性能换语言**；如果决定换，理由是"统一技术栈、减少 C++/Python 两份
   协议实现"，而不是效率。

## 五、实测中发现的 C++ 行为差异（值得记录）

- C++ 服务端对"未入会就发 text_send"的连接会**直接关闭**（日志：`unexpected lobby
  message`）；Python 版是忽略。客户端若有竞态（先发消息后收 join 响应）在 C++ 下会掉线。
- C++ 房间人数上限由创建时 payload 决定（默认 8），压测时若建 8 人房再让 50 人加入，
  会有 42 个 `is full` 拒绝——这与线上"人数上限"语义一致，不是 bug。

## 六、如何复现

```powershell
# 1) 编译 C++ 服务端（WSL）
wsl -e bash /mnt/f/wy/code/qt/CCMeeting/bench/build_cpp.sh

# 2) 启动 C++（8888）与 Python（8899）服务端
wsl -e bash -c "cd /mnt/f/wy/code/qt/CCMeeting/bench && exec ./build/CloudMeetingServer"
wsl -e bash -c "cd /mnt/f/wy/code/qt/CCMeeting/bench && exec python3 py_server.py --port 8899"

# 3) 压测（Windows 端 venv 里已装 websockets==12.0）
python bench_client.py --port 8888 --clients 50 --messages 20 --interval 0.02
python bench_client.py --port 8899 --clients 50 --messages 20 --interval 0.02

# 4) 测服务器 CPU / RSS（WSL 内读 /proc）
wsl -e bash bench/server_stat.sh CloudMeetingServer
wsl -e bash bench/server_stat.sh py_server.py
```

原始结果在 `bench/results/*.json`。
