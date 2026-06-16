# V2/TL-HOC 仿真工程迁移计划

## 1. 背景

当前仓库是在 ns-3 主树上扩展的 `contrib/tl-ocs` 仿真工程。现有实现对应第一版论文 V5/V1，核心是 TL-OCS 光路调度加简单光电协同路由：若跨 ToR flow 命中当前直连 OCS 光路且容量可用，则走 OCS；否则回退到 EPS。

第二版论文 V7/V2 已将方法重新设计为 TL-HOC：结构感知 TL-OCS 光拓扑调度加状态感知协同路由。V2 不再把光路调度结果仅作为直连 OCS 命中规则，而是把结构性通信强度、社区标签、光路调度增益和光链路负载继续传递到逐流路由阶段。

本迁移任务的目标是直接将当前 V1 工程改造成 V2 工程。V1 设计废弃，不保留 V1/V2 切换，不保留 V1 对比实验，不做参数扫描和消融实验。后续所有实现必须以本文档为准；每完成一个 checkpoint，必须更新本文档中的状态和验证结果。

执行规则：本计划执行期间，在当前工作区和已授权命令范围内直接执行实现、验证、文件整理和旧 V1 工程表面替换，不为普通读写、构建、测试、脚本运行另行请求确认。若运行环境或平台权限机制强制要求批准的提权、网络或破坏性操作，按平台机制处理，并在进度日志中记录。

## 2. V1/V2 差异

V1 当前工程特征：

- scheme 包含 `eps-ecmp`、`ocs-volume`、`tl-ocs`、`ocs-oracle`、`fixed-ocs`。
- 算法链路为 `W -> A -> B -> community -> selected OCS edges`。
- `S=max(B,0)` 和完整 `G` 矩阵没有作为一等输出保留。
- 路由层只支持 direct OCS admission；未命中或容量不足时跨组 flow 使用 EPS fallback。
- 输出包含大量 V1 诊断字段，例如 OCS hit rate、oracle comparison、Jaccard、community internal edge ratio、尾部 FCT 等。

V2 目标工程特征：

- 仅保留三种实验策略：`electrical-only`、`static-ocs`、`tl-hoc`。
- 主实验严格使用 128 台服务器、8 个组、每组 16 台服务器、8 x 8 MEMS 光核心。
- 数据面严格实现 no cross-group EPS fallback：跨组 flow 在无可行光核心路径时进入等待队列，不允许回退 EPS。
- 调度链路为 `W -> A -> S -> community -> G -> G_o(t)`，其中 `S=max(B,0)` 和 `G=S*h(c_i,c_j)` 必须显式保存。
- 路由层按顺序尝试 direct optical、structure-related two-hop optical、reachable optical；均不可行时等待重试。
- 延迟安装语义：等待 flow 不提前安装应用，不提前启动流量；retry 成功后才安装并启动。
- 拓扑更新语义：旧 MEMS 光路径物理失效；使用失效路径的 active flow 被中断，未完成字节转为等待 flow；暂不建模重构时延。
- 只采集三个主指标：平均接收端吞吐、平均 flow 完成时间、平均网络链路利用率。

## 3. 工程修改路线

总体策略是分 checkpoint 小步迁移，避免一次性大改。每个 checkpoint 完成后，先更新本文档的状态和验证结果，再进入下一个 checkpoint。

### 3.1 实验表面替换

- 重写 scheme 层，只接受 `electrical-only`、`static-ocs`、`tl-hoc`。
- 删除或改写旧 V1 配置和脚本。
- 旧 scheme 名称应明确报错，避免误跑 V1 实验。

### 3.2 调度状态替换

- 复用当前矩阵、零模型、社区检测等可用代码。
- 将算法输出改造成 V2 状态：`A`、`B`、`S`、community labels、`G`、selected optical topology。
- 删除 V1-only 调度诊断和 V1/V2 对比逻辑。

### 3.3 严格 V2 路由

- 新增核心光拓扑、无向边键、光链路负载状态、协同路由和等待队列模块。
- 跨组 flow 禁止 EPS fallback。
- 无可行光路径时只入队，不安装应用。

### 3.4 延迟安装与重试

- controller timeline 中，flow 到达先执行 TL-HOC route。
- 成功分配路径后才安装 source/sink application。
- retry 成功后以 retry 时间作为实际启动时间。

### 3.5 MEMS 拓扑更新语义

- 拓扑更新时，被移除的旧光边立即不可用于新 flow。
- 已绑定在失效光边上的 active flow 中断。
- 未完成字节生成 residual flow 并进入等待队列。
- 暂不加入 reconfiguration overhead。

### 3.6 最小指标和输出

- summary CSV 只保留实验元信息和三个主指标。
- flow CSV 只保留计算平均 FCT 所需字段。
- link utilization 输出只保留平均网络链路利用率所需字段。
- `validate-results.py` 和 `aggregate-results.py` 按 V2 最小 schema 重写。

### 3.7 主实验

- 配置 128-server、8-group、8 x 8 MEMS 主实验。
- 三个策略使用相同拓扑、相同 flow 序列、相同控制周期。
- 输出单一 summary table，仅比较 `electrical-only`、`static-ocs`、`tl-hoc`。

## 4. Checkpoint 列表

| ID | 名称 | 状态 | 目标 | 主要文件 | 验证命令 | 验证结果 |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | 移除 V1 实验表面 | 已完成 | 只保留 V2 三个 scheme | `scratch/tl-ocs-runner.cc`, `scheme-config.*`, `experiments/configs/*`, `experiments/scripts/*` | `./ns3 build`; `./test.py -s tl-ocs-scheme-config`; `./experiments/scripts/run-v2-smoke.sh <scheme>` | PASS：构建通过；scheme-config 测试通过；三条 V2 smoke 通过；旧 `tl-ocs` scheme 按预期失败 |
| C2 | V2 调度状态 | 已完成 | 输出 `S`、`G`、community、核心光拓扑 | `algorithm/*`, scheduler 相关测试 | `./ns3 build && ./test.py -s tl-ocs-algorithm && ./test.py -s tl-ocs-optical-scheduler` | PASS：构建通过；algorithm 和 optical-scheduler 测试通过；并行 `test.py` 存在汇总文件竞争，已顺序重跑确认通过 |
| C3 | 严格 V2 路由 | 已完成 | 实现无跨组 EPS fallback 的协同路由 | `routing/optical-core-topology.*`, `routing/optical-link-state-manager.*`, `routing/cooperative-router.*`, `routing/wait-queue.*` | `./ns3 build`; `./test.py -s tl-ocs-cooperative-router`; `./test.py -s tl-ocs-wait-queue`; `./test.py -s tl-ocs-optical-link-state` | PASS：构建通过；新增三套 C3 测试通过；`flow-path-selector` 改为 waiting 语义并通过；三条 V2 smoke 通过且 OCS 策略不再产生 EPS fallback |
| C4 | 延迟安装与重试 | 已完成 | retry 成功后才安装并启动应用 | `controller-timeline.*`, `flow-launcher.*` | `./ns3 build`; `./test.py -s tl-ocs-delayed-install`; `./test.py -s tl-ocs-controller-timeline` | PASS：构建通过；delayed-install 和 controller-timeline 测试通过；等待 flow 的 pending demand 进入下一轮调度，retry 成功后才安装 |
| C5 | MEMS 拓扑更新语义 | 已完成 | 旧光路径失效并中断相关 active flow | `controller-timeline.*`, `optical-link-state-manager.*`, `cooperative-router.*` | `./ns3 build`; `./test.py -s tl-ocs-topology-update` | PASS：构建通过；topology-update 测试通过；拓扑更新会停止被移除光边上的 active OCS flow，释放预留并生成 residual waiting flow |
| C6 | 最小指标与输出 | 已完成 | 只输出三个主指标相关数据 | `metrics/*`, `results/*`, `experiments/scripts/validate-results.py`, `aggregate-results.py` | `./experiments/scripts/run-v2-smoke.sh tl-hoc && python3 experiments/scripts/validate-results.py results/raw/v2-tl-hoc-summary.csv results/raw/v2-tl-hoc-flows.csv` | PASS：summary/flow CSV 改为 V2 最小 schema；三策略 smoke artifact 验证通过；聚合表只保留三主指标 |
| C7 | V2 主实验 | 已完成 | 运行三策略 128-server 主实验 | V2 main configs, `run-v2-main.sh` | `./experiments/scripts/run-v2-main.sh` | PASS：三策略 128-server 主实验运行完成；CSV 验证通过；聚合表生成 `results/tables/v2-main-summary.csv` |

状态取值：

- `未开始`
- `进行中`
- `已完成`
- `受阻`
- `需复查`

## 5. 每个 Checkpoint 的验证命令

### C1：移除 V1 实验表面

```bash
./ns3 configure --enable-tests
./ns3 build
./test.py -s tl-ocs-scheme-config
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
```

旧 scheme 名称应明确失败：

```bash
./ns3 run "tl-ocs-runner --schemeName=tl-ocs --enableSchemeRunner=true"
```

### C2：V2 调度状态

```bash
./ns3 build
./test.py -s tl-ocs-algorithm
./test.py -s tl-ocs-optical-scheduler
```

### C3：严格 V2 路由

```bash
./ns3 build
./test.py -s tl-ocs-cooperative-router
./test.py -s tl-ocs-wait-queue
./test.py -s tl-ocs-optical-link-state
```

### C4：延迟安装与重试

```bash
./ns3 build
./test.py -s tl-ocs-delayed-install
```

### C5：MEMS 拓扑更新语义

```bash
./ns3 build
./test.py -s tl-ocs-topology-update
```

### C6：最小指标与输出

```bash
./experiments/scripts/run-v2-smoke.sh tl-hoc
python3 experiments/scripts/validate-results.py \
  results/raw/v2-tl-hoc-summary.csv \
  results/raw/v2-tl-hoc-flows.csv
```

### C7：V2 主实验

```bash
./experiments/scripts/run-v2-main.sh
python3 experiments/scripts/validate-results.py results/raw/v2-*-summary.csv
python3 experiments/scripts/aggregate-results.py results/raw --output results/tables/v2-main-summary.csv
```

## 6. 实验协议

### 6.1 策略

仅运行以下三种策略：

- `electrical-only`：只允许组内电网络和电层策略；跨组光核心不可用时不通过 EPS fallback 表示 TL-HOC。该策略作为纯电基础参考。
- `static-ocs`：使用确定性固定 MEMS 光拓扑，不随流量更新。
- `tl-hoc`：完整 V2，包含结构感知调度和状态感知协同路由。

### 6.2 主实验拓扑

- 服务器总数：128。
- 组数：8。
- 每组服务器数：16。
- 光接入节点数：8。
- 光核心：8 x 8 MEMS。
- 每个组对应一个光接入节点。
- 组内电网络负责服务器接入和组内通信。
- 跨组通信必须进入光核心，不允许 EPS fallback。

### 6.3 流量与启动语义

- flow 定义包含源服务器、目的服务器、大小、开始时间、估计速率。
- 组内 flow 可按组内电路径直接安装。
- 跨组 flow 到达后先执行光核心路径选择。
- 若路径不可用，flow 进入等待队列。
- 等待 flow 在 retry 成功前不得安装应用，不得启动流量。
- retry 成功后，以 retry 成功时刻安装并启动应用。

### 6.4 拓扑更新语义

- `tl-hoc` 每个调度周期生成新的 MEMS 光拓扑。
- 新拓扑生效时，旧拓扑中被移除的光边立即物理失效。
- 失效光边上的 active flow 中断。
- 中断 flow 的未完成字节生成 residual flow 并进入等待队列。
- 暂不建模 MEMS 重构时延。

### 6.5 `static-ocs` 固定拓扑

- 使用确定性 ring。
- 若光端口约束允许，再加入确定性 matching。
- 不做流量自适应更新。
- 不做参数扫描。

### 6.6 指标

只输出和计算以下三项：

1. 平均接收端吞吐：

```text
avg_receiver_throughput_bps =
    average over receivers of received_bytes(receiver) * 8 / measurement_duration_s
```

2. 平均 flow 完成时间：

```text
avg_fct_s =
    average over completed flows of completion_time_s - actual_start_time_s
```

3. 平均网络链路利用率：

```text
avg_link_utilization =
    average over measured links of transmitted_bytes(link) * 8 /
    (capacity_bps(link) * measurement_duration_s)
```

暂不输出：

- 参数敏感性结果。
- 消融实验结果。
- V1/V2 对比。
- OCS hit rate、byte hit rate。
- p90/p95 FCT。
- oracle/volume/Jaccard 诊断。
- community internal selected edge ratio。

## 7. 风险清单

1. 多跳光路径数据面安装复杂：当前 `FlowPathSelector` 主要支持 direct OCS 地址改写，多跳光路径可能需要额外静态路由或控制层模拟。
2. no cross-group EPS fallback 可能导致完成率下降：这是 V2 论文语义要求，不能通过 EPS fallback 掩盖。
3. 延迟安装会改变 flow 实际开始时间：FCT 必须以实际应用启动时间计算，而不是原始计划到达时间。
4. 拓扑更新中断 active flow 需要残余字节估计：若只能按接收字节估算，需记录估算方法。
5. ns-3 事件顺序会影响结果：同一时间点的窗口快照、调度、flow 到达和 retry 必须固定顺序。
6. 等待队列重试频率过高可能导致事件数量膨胀：需要 batch 和 retry interval 限制。
7. 删除旧 V1 输出字段会影响旧验证脚本：必须同步重写 `validate-results.py` 和 `aggregate-results.py`。
8. V2 主实验规模较大，运行时间可能较长：先用 smoke 确认，再跑完整主实验。
9. `electrical-only` 与 no cross-group EPS fallback 的语义边界需要在实现中清晰区分：`electrical-only` 是对照策略，不代表 TL-HOC fallback。

## 8. 当前进度日志格式

每完成一个 checkpoint，必须在本节追加一条日志，并同步更新第 4 节表格中的状态和验证结果。

日志格式：

```text
### YYYY-MM-DD HH:MM Checkpoint Cx: <名称>

状态：
- 未开始 / 进行中 / 已完成 / 受阻 / 需复查

本次修改：
- <文件或模块级摘要>

验证命令：
```bash
<实际执行的命令>
```

验证结果：
- PASS / FAIL / SKIPPED
- <关键输出或失败原因>

残留问题：
- <无 / 具体问题>

下一步：
- <下一个 checkpoint 或修复项>
```

## 9. 当前进度日志

### 2026-06-16 PLAN 初始化

状态：
- 已完成

本次修改：
- 新增 `PLAN.md`。
- 将 V2 全量替换范围、checkpoint、验证命令、实验协议、风险和日志格式固化为后续实现准则。

验证命令：

```bash
未运行；本 checkpoint 仅新增计划文档。
```

验证结果：
- SKIPPED

残留问题：
- 无。

下一步：
- 从 Checkpoint C1 开始移除 V1 实验表面。

### 2026-06-16 16:24 CST Checkpoint C1：移除 V1 实验表面

状态：
- 已完成

本次修改：
- 将 `SchemeConfig` 改为只接受 `electrical-only`、`static-ocs`、`tl-hoc`。
- 更新 `tl-ocs-scheme-config` 测试，验证旧 V1 scheme 名称全部失败。
- 删除旧 V1 smoke/metrics/sanity 配置，新增三份 V2 smoke 配置。
- 删除旧 V1 metrics/util/sanity 运行脚本，保留并改写 scheme smoke 入口，新增 `run-v2-smoke.sh` 和 `run-v2-main.sh` 占位入口。
- 修正 `electrical-only` smoke status，避免继续输出旧 `eps-ecmp` 状态名。
- 更新 `validate-results.py` 中的 scheme 集合，移除旧 `ocs-volume` / `tl-ocs` 引用。

验证命令：

```bash
./ns3 configure --enable-tests
./ns3 build
./test.py -s tl-ocs-scheme-config
./ns3 run "tl-ocs-runner --schemeName=tl-ocs --enableSchemeRunner=true"
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`tl-ocs-scheme-config` 测试通过。
- PASS：旧 `tl-ocs` scheme 返回 `unknown TL-HOC V2 scheme: tl-ocs`，符合预期。
- PASS：`electrical-only`、`static-ocs`、`tl-hoc` 三条 V2 smoke 均运行完成并写出 CSV。

残留问题：
- C1 只替换实验表面；`static-ocs` 和 `tl-hoc` 内部仍临时复用现有 V1 controller/routing 骨架，严格 no cross-group EPS fallback、延迟安装、MEMS 拓扑失效语义将在 C3-C5 完成。
- 结果 CSV 仍包含旧 V1 指标字段，最小指标 schema 将在 C6 完成。

下一步：
- 进入 Checkpoint C2：将算法输出改造成 V2 调度状态，显式保留 `S`、`G`、community 和核心光拓扑所需信息。

### 2026-06-16 16:31 CST Checkpoint C2：V2 调度状态

状态：
- 已完成

本次修改：
- 在 `OpticalScheduleResult` 中新增 `scheduleGain` 和 `selectedDegree`，并在调度器中保存完整候选调度增益矩阵。
- 在 `OpticalSchedulerParameters` 中新增 `maxOpticalLinks`，用于后续 8 x 8 MEMS 全局链路数量约束；默认值 `0` 表示不限制，保持当前测试和 smoke 行为。
- 在 `TlOcsAlgorithmResult` 中显式输出 `S=max(B,0)`、`G` 和 `selectedDegree`，为 C3 的状态感知协同路由提供输入。
- 扩展 `tl-ocs-algorithm` 与 `tl-ocs-optical-scheduler` 测试，覆盖 `S`、`G`、节点已选度数和全局选边上限。

验证命令：

```bash
./ns3 build
./test.py -s tl-ocs-algorithm
./test.py -s tl-ocs-optical-scheduler
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`tl-ocs-algorithm` 测试通过。
- PASS：`tl-ocs-optical-scheduler` 测试通过。
- 注意：曾并行执行两个 `test.py` 进程，其中 `tl-ocs-optical-scheduler` 测试本体 PASS，但 `test.py` 汇总 XML 文件发生竞争导致退出码异常；顺序重跑后通过。

残留问题：
- C2 只完成 V2 调度状态显式化；数据面仍未实现 no cross-group EPS fallback。
- `maxOpticalLinks` 已具备参数入口，但 128-server / 8 x 8 MEMS 主实验约束会在后续配置和拓扑模块中接入。

下一步：
- 进入 Checkpoint C3：实现严格 V2 路由，新增光核心拓扑、链路状态、协同路由和等待队列。

### 2026-06-16 16:47 CST Checkpoint C3：严格 V2 路由

状态：
- 已完成

本次修改：
- 新增 `OpticalCoreTopology`，用于表示当前 MEMS 光核心的物理无向光边，并支持邻居和路径查询。
- 新增 `OpticalLinkStateManager`，按无向光边维护已预留速率，只有路径上所有物理光边存在且容量足够时才允许预留。
- 新增 `CooperativeRouter`，按 V2 顺序尝试 `optical-direct`、`optical-two-hop`、`optical-reachable`；均不可行时返回 `waiting`，不提供 EPS fallback。
- 新增 `WaitQueue`，保存 blocked flow、失败原因、入队时间和 retry 次数。
- 将旧 `FlowPathSelector` 的跨组失败语义从默认 `eps` 改为 `waiting/installable=false`。
- 修改 `FlowLauncher`，跳过 `installable=false` 的 flow，避免等待 flow 提前安装 source/sink application。
- 更新 `tl-ocs-flow-path-selector` 测试，删除容量不足和光路关闭后的 EPS fallback 期望。

验证命令：

```bash
./ns3 build
./test.py -s tl-ocs-cooperative-router
./test.py -s tl-ocs-wait-queue
./test.py -s tl-ocs-optical-link-state
./test.py -s tl-ocs-flow-path-selector
./test.py -s tl-ocs-ocs-admission
./test.py -s tl-ocs-flow-launcher
./test.py -s tl-ocs-flow-launcher-metrics
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`tl-ocs-cooperative-router`、`tl-ocs-wait-queue`、`tl-ocs-optical-link-state` 均通过。
- PASS：`tl-ocs-flow-path-selector` 通过，确认 inactive/capacity-exceeded cross-group flow 进入 waiting 且不安装应用。
- PASS：`tl-ocs-ocs-admission`、`tl-ocs-flow-launcher`、`tl-ocs-flow-launcher-metrics` 均通过。
- PASS：三条 V2 smoke 均通过；`static-ocs` 和 `tl-hoc` 的 `epsFallback=0`。

残留问题：
- `CooperativeRouter` 已具备 direct/two-hop/reachable 逻辑，但 controller/timeline 当前仍主要通过旧 direct OCS selector 安装 flow；C4 需要把 wait queue、retry 和实际安装流程接入 timeline。
- 等待 flow 当前不会安装，因此 smoke 的 flow metrics 只包含实际启动的 flow；C6 会收敛最终最小指标 schema。

下一步：
- 进入 Checkpoint C4：实现 delayed installation / retry，确保等待 flow 只有在 retry 成功后才安装并启动。

### 2026-06-16 17:06 CST Checkpoint C4：延迟安装与重试

状态：
- 已完成

本次修改：
- 在 `ControllerTimelineResult` 中增加 `waitingFlows` 和 `retriedFlows`，显式记录等待与 retry 成功情况。
- 有限周期 controller 中，flow 到达后若 `FlowPathSelector` 返回 `waiting/installable=false`，只进入 `WaitQueue`，不安装 source/sink application。
- 每轮 MEMS 拓扑调度更新后重试等待队列；retry 成功时用当前 `Simulator::Now()` 重建 `FlowSpec` 并安装应用，因此 flow metric 的 `startTimeS` 反映实际启动时间。
- 将等待队列中的 pending demand 合并进下一轮调度矩阵，避免等待 flow 因未发送数据而永远不被 observer 捕获。
- 调度边界包含 `trafficStopTime`，允许 trafficStop 前已到达的等待 flow 在边界处做最后一次 retry。
- 新增 `tl-ocs-delayed-install` 测试，验证 retry 成功前不安装、retry 成功后按当前时间启动且不产生 EPS fallback。
- 更新 `tl-ocs-controller-timeline` 旧测试，移除 V1 EPS fallback 期望，改为 V2 waiting/retry 语义。

验证命令：

```bash
./ns3 build
./test.py -s tl-ocs-delayed-install
./test.py -s tl-ocs-controller-timeline
./test.py -s tl-ocs-wait-queue
./test.py -s tl-ocs-flow-path-selector
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`tl-ocs-delayed-install` 通过，确认等待 flow 在 10ms topology update/retry 后才安装，metric start time 为 retry 时间。
- PASS：`tl-ocs-controller-timeline` 通过，确认有限周期 controller 采用 waiting/retry 语义。
- PASS：`tl-ocs-wait-queue` 和 `tl-ocs-flow-path-selector` 通过。
- PASS：三条 V2 smoke 均通过；`static-ocs` 和 `tl-hoc` 的旧 `epsFallback` 字段为 0。

残留问题：
- `electrical-only` 仍使用旧字段名 `epsFallback` 表示纯电策略安装的 EPS flow；C6 会删除或重命名旧 V1 输出字段。
- 当前 timeline 的实际数据面仍以 direct OCS host route 为主；`CooperativeRouter` 的 two-hop/reachable 逻辑已单元验证，但多跳数据面安装需要在后续 checkpoint 中继续收敛。

下一步：
- 进入 Checkpoint C5：实现 MEMS 拓扑更新时旧光路径物理失效和 active flow 中断/残余等待语义。

### 2026-06-16 17:16 CST Checkpoint C5：MEMS 拓扑更新语义

状态：
- 已完成

本次修改：
- 在有限周期 controller 内跟踪 active OCS flow，包括原始 flow、路径决策、metric tracking 和 source application container。
- 正常完成回调中释放 OCS 预留并移除 active flow 状态。
- 每轮拓扑更新时比较 old/new active OCS edge；被移除光边上的 active OCS flow 会被立即停止 source application。
- 中断 flow 会释放 OCS 预留；若未完成，则按未接收字节生成 residual flow，入 `WaitQueue` 等待后续 retry。
- 新增 `interruptedFlows` 和 `residualFlows` 结果计数。
- 新增 `tl-ocs-topology-update` 测试，验证旧光边移除会中断 active flow、生成 residual flow，且不产生 EPS fallback。

验证命令：

```bash
./ns3 build
./test.py -s tl-ocs-topology-update
./test.py -s tl-ocs-delayed-install
./test.py -s tl-ocs-controller-timeline
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`tl-ocs-topology-update` 通过，确认 active OCS flow 在旧光边移除后被中断并生成 residual waiting flow。
- PASS：`tl-ocs-delayed-install` 和 `tl-ocs-controller-timeline` 通过。
- PASS：三条 V2 smoke 均通过；光策略仍无 EPS fallback。

残留问题：
- C5 已建模建立/拆除导致的路径有效性变化，但未加入 MEMS reconfiguration overhead，符合当前计划。
- residual flow 使用新的内部 flow id；C6 汇总最小指标时需要决定是否将 residual flow 作为独立完成记录，或按原 flow 聚合。

下一步：
- 进入 Checkpoint C6：收敛最小指标与输出 schema，只保留平均接收端吞吐、平均 FCT、平均链路利用率相关数据。

### 2026-06-16 19:48 CST Checkpoint C6：最小指标与输出

状态：
- 已完成

本次修改：
- 将 summary CSV schema 收敛为 V2 最小字段：实验元信息、`total_flows`、`completed_flows`、`avg_receiver_throughput_bps`、`avg_fct_s`、`avg_network_link_utilization`。
- 将 flow CSV schema 收敛为 FCT/吞吐所需字段：`flow_id`、`received_bytes`、`start_time_s`、`completion_time_s`、`fct_s`、`completed`。
- 在 `LinkUtilizationSummary` 中新增 `avgNetworkLinkUtilization`，按 EPS 链路和 active OCS 链路统一平均。
- 更新 `validate-results.py`，只接受 V2 三个 scheme 和最小 schema。
- 更新 `aggregate-results.py`，聚合表只保留三项主指标。
- 更新三份 V2 smoke 配置，启用 `enableLinkMetrics=true`，确保第三个主指标不为空。
- 将 `tl-ocs-smoke-scenario-runner` 测试重写为 V2 三策略测试，移除 V1 scheme 和 baseline 断言。

验证命令：

```bash
./ns3 build
./test.py -s tl-ocs-result-writer
./test.py -s tl-ocs-flow-result-writer
./test.py -s tl-ocs-link-utilization-metrics
./test.py -s tl-ocs-smoke-scenario-runner
./experiments/scripts/run-v2-smoke.sh electrical-only
./experiments/scripts/run-v2-smoke.sh static-ocs
./experiments/scripts/run-v2-smoke.sh tl-hoc
python3 experiments/scripts/validate-results.py \
  results/raw/v2-electrical-only-summary.csv \
  results/raw/v2-electrical-only-flows.csv \
  results/raw/v2-static-ocs-summary.csv \
  results/raw/v2-static-ocs-flows.csv \
  results/raw/v2-tl-hoc-summary.csv \
  results/raw/v2-tl-hoc-flows.csv
python3 experiments/scripts/aggregate-results.py results/raw \
  --output results/tables/v2-smoke-summary.csv
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：result writer、flow result writer、link utilization、smoke scenario runner 测试通过。
- PASS：三策略 V2 smoke 均运行完成。
- PASS：六个 summary/flow CSV artifact 均通过 V2 schema 验证。
- PASS：聚合脚本生成 `results/tables/v2-smoke-summary.csv`。

残留问题：
- runner 的控制台日志仍打印部分旧诊断名称，例如 `epsFallback`、`p95FctS`；C6 已保证 CSV artifact 不再输出这些字段。
- `avg_network_link_utilization` 当前按所有 EPS 链路和 active OCS 链路的 utilization 统一平均，未按链路持续时间重新加权。

下一步：
- 进入 Checkpoint C7：配置并运行 128-server 三策略 V2 主实验，生成 `results/tables/v2-main-summary.csv`。

### 2026-06-16 19:52 CST Checkpoint C7：V2 主实验

状态：
- 已完成

本次修改：
- 新增三份主实验配置：`v2-main-electrical-only.properties`、`v2-main-static-ocs.properties`、`v2-main-tl-hoc.properties`。
- 主实验固定为 8 个 ToR/group、每组 16 台服务器，总计 128 servers。
- 光策略使用 8 条 MEMS 光核心链路规模；`static-ocs` 使用确定性 ring，`tl-hoc` 使用有限周期调度。
- 主实验启用 flow metrics 和 link metrics，只输出 C6 最小 schema。
- 将 `run-v2-main.sh` 从占位脚本改为完整主实验入口：运行三策略、验证 CSV、聚合主结果表。

验证命令：

```bash
./ns3 build
./experiments/scripts/run-v2-main.sh
cat results/tables/v2-main-summary.csv
```

验证结果：
- PASS：`./ns3 build` 通过。
- PASS：`./experiments/scripts/run-v2-main.sh` 完成三策略运行、CSV 验证和聚合。
- PASS：生成 `results/tables/v2-main-summary.csv`。

主实验结果：

```text
electrical-only: throughput=512000000 bps, avg_fct=0.00079608 s, avg_link_util=0.000988945066667
static-ocs:      throughput=384000000 bps, avg_fct=0.000550194125 s, avg_link_util=0.000086207847619
tl-hoc:          throughput=512000000 bps, avg_fct=0.000550123488281 s, avg_link_util=0.000227181511111
```

结论：
- 本次快速主实验中，`tl-hoc` 与 `electrical-only` 的平均接收端吞吐相同，同时平均 FCT 明显低于 `electrical-only`。
- `static-ocs` 的平均 FCT 接近 `tl-hoc`，但 no-fallback 语义下只完成/安装了 192 条 flow，对应吞吐低于 `tl-hoc`。

残留问题：
- 当前主实验仍是快速验证规模，尚未做多 seed 或更长运行时间。
- `static-ocs` 与 `tl-hoc` 的 flow 完成数差异会影响吞吐解释；后续可增加完成率字段，但当前计划只保留三项主指标。

下一步：
- 当前 PLAN 的 C1-C7 已全部完成。后续若继续迭代，建议优先做多 seed 稳定性验证和更严格的 8x8 MEMS 物理约束审计。
