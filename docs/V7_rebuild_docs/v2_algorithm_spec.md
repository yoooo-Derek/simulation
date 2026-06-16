# v2_algorithm_spec.md

# V2 / V7 Algorithm Engineering Specification: TL-HOC

> 目的：将第二版论文 V7 中的 TL-HOC（Traffic Locality-aware Hybrid Optical-electrical Control）写成可直接交给 Codex 改造仿真工程的工程实现规格。
>
> 本文不是论文叙述，而是实现说明。所有模块、数据结构、函数接口、伪代码、异常处理和默认参数均围绕仿真工程落地组织。
>
> 重要说明：V7 给出了算法变量、公式、约束、流程和实验规模，但未给出所有参数的具体数值。因此本规格中标注为“工程默认建议”的数值不是论文结论，只是为了让仿真工程可以先运行、可配置、可复现实验。

---

## 1. 算法总体定义

### 1.1 算法名称

第二版算法名称建议在工程中统一为：

```text
TL-HOC = TL-OCS Scheduler + State-Aware Cooperative Routing
```

其中：

- `TL-OCS Scheduler`：结构感知动态光拓扑调度模块。
- `State-Aware Cooperative Routing`：状态感知协同路由模块。
- `TL-HOC Controller`：调度、路由、等待队列、链路负载和统计窗口的总控模块。

### 1.2 控制链路

V2 的完整控制链路为：

```text
W^(t-1)
 -> A^(t-1)
 -> S^(t-1)
 -> C^(t-1)
 -> G^(t-1)
 -> G_o(t)
 -> path(f)
```

工程含义：

| 阶段 | 工程对象 | 说明 |
|---|---|---|
| `W^(t-1)` | `TrafficMatrix directed_bytes` | 上一统计窗口的有向光接入节点流量矩阵。 |
| `A^(t-1)` | `TrafficMatrix undirected_bytes` | 双向合并后的无向通信强度矩阵。 |
| `S^(t-1)` | `Matrix structural_strength` | 结构性通信强度矩阵，等于模块度增益正向部分。 |
| `C^(t-1)` | `vector<int> community_id` | Louvain 输出的社区标签。 |
| `G^(t-1)` | `Matrix schedule_gain` | 光路调度增益矩阵。 |
| `G_o(t)` | `CoreOpticalTopology` | 当前调度周期生效的核心光拓扑。 |
| `path(f)` | `RouteResult` | 新到达数据流的端到端路径或等待状态。 |

### 1.3 V2 与 V1 的关键工程差异

V1 的路由层主要是：如果源宿接入节点命中当前直连光路且容量允许，则走 OCS；否则走 EPS。V2 需要改成：

```text
组内流：走组内电路径。
跨组流：在当前核心光拓扑 G_o(t) 中依次尝试：
    1. 直接光路径；
    2. 结构相关两跳光路径；
    3. 一般可达光路径；
若均不可行：进入等待队列 Q(t)，等待释放、重构或定时重试。
```

V2 的路由必须继承 TL-OCS 输出的：

- 社区标签 `community_id[v]`；
- 光路调度增益 `schedule_gain[u][v]`；
- 当前光链路负载 `optical_load[(u,v)]`；
- 光链路阈值 `theta_o`；
- 最大核心搜索跳数 `h_max`。

---

## 2. 网络模型与工程对象

### 2.1 分层拓扑

V2 的网络不再只建模为 EPS + OCS 扁平结构，而应建模为：

```text
G(t) = (V_s, V_e, V_o, E_intra_e, E_o(t))
```

工程含义：

| 符号 | 工程对象 | 说明 |
|---|---|---|
| `V_s` | `vector<ServerId> servers` | 服务器集合。 |
| `V_e` | `vector<ElectricalNodeId> electrical_nodes` | 组内电交换节点集合。 |
| `V_o` | `vector<OpticalNodeId> optical_nodes` | 光接入节点集合。每个组可对应一个或多个光接入节点，主实验为每组一个。 |
| `E_intra_e` | `vector<ElectricalLink>` | 组内电链路集合。用于服务器接入、本地转发、服务器到光接入节点。 |
| `E_o(t)` | `vector<OpticalLink>` | 当前周期 TL-OCS 生成的核心光路集合。 |

### 2.2 服务器到光接入节点映射

论文中使用函数：

```text
g(h) -> v
```

工程实现：

```cpp
using ServerId = uint32_t;
using OpticalNodeId = uint32_t;
using GroupId = uint32_t;

struct ServerMapping {
    unordered_map<ServerId, GroupId> server_to_group;
    unordered_map<GroupId, vector<OpticalNodeId>> group_to_optical_nodes;
    unordered_map<ServerId, OpticalNodeId> server_to_primary_optical;
};
```

最小实现可只支持每组一个光接入节点：

```cpp
OpticalNodeId GetOpticalNode(ServerId h) {
    return server_to_primary_optical.at(h);
}
```

异常要求：

- 若 `h` 不存在，返回 `RouteStatus::INVALID_SERVER_MAPPING`，不要崩溃。
- 若一个组有多个光接入节点但未配置选择策略，默认选择 `group_to_optical_nodes[group][0]`，并记录 warning。

---

## 3. 核心数据结构

### 3.1 基础 ID 与边键

光路为无向边。工程中必须使用规范化边键，避免 `(u,v)` 与 `(v,u)` 产生两份状态。

```cpp
using NodeId = uint32_t;
using ServerId = uint32_t;
using FlowId = uint64_t;
using TimeNs = uint64_t;
using Bytes = double;
using Rate = double;  // bit/s or byte/s, 全工程必须统一

struct EdgeKey {
    NodeId a;
    NodeId b;

    EdgeKey(NodeId u, NodeId v) {
        if (u <= v) { a = u; b = v; }
        else { a = v; b = u; }
    }

    bool IsSelfLoop() const { return a == b; }
};
```

约束：

```text
EdgeKey(u,v) == EdgeKey(v,u)
禁止 EdgeKey(u,u) 作为光路。
```

### 3.2 流对象

论文流定义：

```text
f = (h_s, h_d, L_f, r_f)
```

工程实现：

```cpp
struct Flow {
    FlowId id;
    ServerId src_server;
    ServerId dst_server;
    Bytes size_bytes;       // L_f
    Rate rate_estimate;     // r_f，用于负载判断
    TimeNs start_time;
    TimeNs deadline = 0;    // 可选，0 表示不使用 deadline
    uint32_t retry_count = 0;
};
```

异常要求：

- `size_bytes <= 0`：拒绝路由，返回 `INVALID_FLOW_SIZE`。
- `rate_estimate <= 0`：按配置处理：
  - `infer_rate_if_missing=true` 时，根据 `size_bytes / default_flow_duration` 估算；
  - 否则返回 `INVALID_RATE_ESTIMATE`。

### 3.3 矩阵结构

```cpp
struct DenseMatrix {
    uint32_t n;
    vector<double> data; // row-major, size = n*n

    double& operator()(uint32_t i, uint32_t j);
    double  operator()(uint32_t i, uint32_t j) const;
};
```

需要维护以下矩阵：

| 矩阵 | 名称 | 含义 |
|---|---|---|
| `W` | `directed_bytes` | 有向流量矩阵，`W[i][j]` 为窗口内 `v_i -> v_j` 的通信量。 |
| `A` | `undirected_bytes` | 无向通信强度，`A[i][j] = W[i][j] + W[j][i]`。 |
| `P` | `expected_bytes` | 随机图零模型期望通信强度。 |
| `B` | `modularity_gain` | 模块度增益矩阵，可正可负。 |
| `S` | `structural_strength` | 结构性通信强度，`S=max(B,0)`。 |
| `G` | `schedule_gain` | 光路调度增益，`G=S*h(c_i,c_j)`。 |

对于 `N <= 256`，可直接使用 dense matrix。若后续仿真扩展到更大 `N`，可切换 sparse map。

### 3.4 光拓扑快照

```cpp
struct OpticalLinkState {
    EdgeKey edge;
    Rate capacity;          // C_o
    Rate threshold;         // Theta_o
    Rate current_load;      // L_ij^o(t)
    double schedule_gain;   // G_ij^(t-1)
    bool active;
};

struct CoreOpticalTopology {
    uint64_t epoch; // 调度周期编号 t
    vector<NodeId> optical_nodes;
    unordered_set<EdgeKey> edges;
    unordered_map<EdgeKey, OpticalLinkState> link_state;
    unordered_map<NodeId, vector<NodeId>> adjacency;
};
```

约束：

- `edges` 与 `link_state` 必须一致。
- `adjacency` 从 `edges` 派生，不要手动维护多份不一致状态。
- 每次调度周期生成新拓扑时，必须生成新的 `epoch`。

### 3.5 调度输出

```cpp
struct SchedulerOutput {
    uint64_t epoch;
    CoreOpticalTopology topology;             // G_o(t)
    DenseMatrix x;                            // x_ij^(t), 0/1
    vector<int> community_id;                 // c_i
    DenseMatrix schedule_gain;                // G_ij^(t-1)
    DenseMatrix structural_strength;          // S_ij^(t-1), 可用于 debug
    vector<uint32_t> optical_degree;          // deg_o^(t)(v_i)
};
```

### 3.6 路由路径与结果

```cpp
enum class PathType {
    INTRA_ELECTRICAL,
    DIRECT_OPTICAL,
    STRUCTURE_RELATED_INDIRECT_OPTICAL,
    REACHABLE_OPTICAL,
    WAIT_RETRY,
    FAILED
};

struct OpticalPath {
    vector<NodeId> nodes;      // e.g. [v_s, v_r, v_d]
    vector<EdgeKey> edges;     // derived from nodes
    PathType type;
    uint32_t hop_count;        // H(p)
    double structure_score;    // Gamma(p)
    Rate max_link_load;        // L(p)
};

struct EndToEndPath {
    vector<uint32_t> electrical_prefix_nodes; // h_s -> v_s
    OpticalPath core_path;                    // path_o(v_s,v_d)
    vector<uint32_t> electrical_suffix_nodes; // v_d -> h_d
    vector<uint32_t> intra_electrical_nodes;  // only for group-internal flow
};

struct RouteResult {
    FlowId flow_id;
    PathType type;
    bool success;
    bool queued;
    string reason;
    EndToEndPath path;
};
```

### 3.7 等待队列

```cpp
enum class RetryReason {
    LINK_RELEASED,
    TOPOLOGY_UPDATED,
    PERIODIC_RETRY
};

struct WaitingFlowEntry {
    Flow flow;
    TimeNs enqueue_time;
    uint64_t enqueue_epoch;
    string last_failure_reason;
};

class WaitQueue {
public:
    bool Enqueue(const Flow& f, string reason);
    vector<Flow> PopRetryBatch(RetryReason reason, uint32_t max_batch);
    bool Remove(FlowId id);
    size_t Size() const;
};
```

队列语义：

- 只保存跨组流。
- 组内电流不进入等待队列。
- 入队时不更新任何光链路负载。
- 重试成功后才绑定路径并更新光链路负载。
- 重试失败时保留在队列中，更新 `retry_count` 和 `last_failure_reason`。

---

## 4. 配置参数与默认值

### 4.1 调度配置

```cpp
struct SchedulerConfig {
    uint32_t k = 2;                         // 每个光接入节点端口上限，工程默认建议
    optional<uint32_t> max_optical_links;   // K_o；默认 null，不额外限制
    Rate optical_capacity = 100e9;          // C_o，工程默认建议：100 Gbps
    double eta = 1.0;                       // 分辨率参数
    double alpha = 0.5;                     // 跨社区折减系数，必须在 (0,1)
    uint32_t louvain_max_iter = 20;         // 工程默认建议
    double louvain_min_delta_q = 1e-9;      // 工程默认建议
    bool require_cross_group = true;        // 候选光路仅允许跨组
    bool keep_existing_flows_on_old_path = true;
};
```

参数校验：

| 参数 | 合法范围 | 异常处理 |
|---|---:|---|
| `k` | `>= 0` | 若 `k=0`，生成空光拓扑，但路由仍可处理组内流；跨组流进入等待队列或 fallback。 |
| `max_optical_links` | `>= 0` | 若为 0，等价于不建立光路。 |
| `optical_capacity` | `> 0` | 非法则停止初始化。 |
| `eta` | `> 0` | 非法则使用 `1.0` 并记录 error，或按 strict 模式拒绝。 |
| `alpha` | `(0,1)` | 非法则 clamp 到 `[1e-6, 1-1e-6]` 或 strict 模式拒绝。 |
| `louvain_max_iter` | `>= 1` | 非法则使用 20。 |

### 4.2 路由配置

```cpp
struct RoutingConfig {
    double theta_ratio = 0.8;               // Theta_o = theta_ratio * C_o，工程默认建议
    optional<Rate> optical_threshold;       // 若显式配置，则覆盖 theta_ratio
    uint32_t h_max = 3;                     // 一般可达光路径最大核心跳数，工程默认建议
    bool enable_wait_queue = true;
    bool fallback_to_electrical_for_cross_group = false;
    TimeNs retry_interval = 10'000'000;     // 10 ms，工程默认建议
    uint32_t retry_max_batch = 1024;
    uint32_t max_retry_count = 0;           // 0 表示不限次数
    bool infer_rate_if_missing = true;
    TimeNs default_flow_duration = 100'000'000; // 100 ms，工程默认建议
};
```

`Theta_o` 计算规则：

```cpp
if (optical_threshold.has_value()) {
    theta_o = optical_threshold.value();
} else {
    theta_o = theta_ratio * optical_capacity;
}
theta_o = min(theta_o, optical_capacity);
```

### 4.3 实验默认配置

V7 主实验给出的规模可作为工程默认：

```yaml
num_servers: 128
num_groups: 8
servers_per_group: 16
num_optical_nodes: 8
k: 2
rho_values: [0.3, 0.5, 0.7, 0.9]
traffic_scenarios:
  - uniform_background
  - community_training
  - aggregation_training
```

未在 V7 中明确给出数值的参数建议统一放入配置文件，不要硬编码在算法内部。

---

## 5. 模块边界

### 5.1 模块总览

```text
TLHocController
├── TopologyManager
├── TrafficObserver
├── StructureAnalyzer
├── TLOcsScheduler
├── OpticalLinkStateManager
├── CooperativeRouter
├── WaitQueueManager
├── FlowLifecycleManager
└── MetricsCollector
```

### 5.2 TopologyManager

职责：

- 维护服务器、组内电网络、光接入节点、核心光拓扑；
- 提供 `g(h)` 映射；
- 提供组内电路径 `path_e(...)`；
- 接收 TL-OCS 输出并更新当前 `CoreOpticalTopology`。

核心接口：

```cpp
class TopologyManager {
public:
    OpticalNodeId GetOpticalNode(ServerId server) const;
    GroupId GetGroupOfServer(ServerId server) const;
    GroupId GetGroupOfOpticalNode(NodeId optical_node) const;

    vector<uint32_t> GetIntraElectricalPath(ServerId src, ServerId dst) const;
    vector<uint32_t> GetAccessPath(ServerId server, NodeId optical_node) const;

    void ApplyOpticalTopology(const CoreOpticalTopology& topology);
    const CoreOpticalTopology& CurrentOpticalTopology() const;
};
```

### 5.3 TrafficObserver

职责：

- 在统计窗口内收集流量；
- 将服务器级流聚合成光接入节点级 `W`；
- 在窗口结束时生成 `A`。

接口：

```cpp
class TrafficObserver {
public:
    void OnFlowObserved(const Flow& f, const ServerMapping& mapping);
    DenseMatrix GetDirectedMatrixAndReset();
    DenseMatrix BuildUndirectedMatrix(const DenseMatrix& W) const;
};
```

实现要求：

```text
if g(src_server) == g(dst_server):
    不计入跨光接入节点矩阵，或只保留 w_ii=0。
else:
    W[v_s][v_d] += L_f
```

### 5.4 StructureAnalyzer

职责：

- 计算 `d_i`、`M`、`P`、`B`、`S`；
- 执行 Louvain 社区划分；
- 计算社区内局部性 `L(C)` 和社区间关联 `R(Cp,Cq)`，用于 debug/指标，不直接决定光路选择；
- 计算光路调度增益 `G`。

接口：

```cpp
struct StructureResult {
    vector<double> weighted_degree;      // d_i
    double total_effective_traffic;      // M
    DenseMatrix expected;                // P
    DenseMatrix modularity_gain;         // B
    DenseMatrix structural_strength;     // S
    vector<int> community_id;            // c_i
    DenseMatrix schedule_gain;           // G
    unordered_map<int, double> locality_gain_by_community; // L(C)
    unordered_map<pair<int,int>, double> inter_community_relation; // R(Cp,Cq)
};

class StructureAnalyzer {
public:
    StructureResult Analyze(const DenseMatrix& A,
                            const vector<NodeId>& optical_nodes,
                            const SchedulerConfig& config);
};
```

### 5.5 TLOcsScheduler

职责：

- 根据 `StructureResult` 构造候选光路集合；
- 按 `G_ij` 降序排序；
- 在端口约束和可选 `K_o` 约束下贪心选边；
- 初始化核心光拓扑、容量状态和负载状态。

接口：

```cpp
class TLOcsScheduler {
public:
    SchedulerOutput Schedule(uint64_t epoch,
                             const DenseMatrix& A_prev,
                             const vector<NodeId>& optical_nodes,
                             const TopologyManager& topology,
                             const SchedulerConfig& config,
                             const RoutingConfig& routing_config);
};
```

### 5.6 OpticalLinkStateManager

职责：

- 保存当前光链路负载；
- 在路由绑定时增加负载；
- 在流完成时释放负载；
- 判断路径可行性。

接口：

```cpp
class OpticalLinkStateManager {
public:
    bool IsPathFeasible(const OpticalPath& p, Rate r_f) const;
    Rate GetLoad(EdgeKey e) const;
    Rate GetThreshold(EdgeKey e) const;

    bool ReservePath(FlowId flow_id, const OpticalPath& p, Rate r_f);
    bool ReleasePath(FlowId flow_id);

    vector<FlowId> GetFlowsOnEdge(EdgeKey e) const;
};
```

异常要求：

- `ReservePath` 必须二阶段检查：先检查所有边可行，再统一更新，避免半更新。
- `ReleasePath` 遇到未知 `flow_id` 时返回 false 并记录 warning。
- 释放后负载不能小于 0；若出现浮点误差，允许 clamp 到 0。

### 5.7 CooperativeRouter

职责：

- 对新到达流进行逐流路由；
- 组内流走电路径；
- 跨组流构造三类核心光路径；
- 按路径适配度选择；
- 无可行路径时入等待队列。

接口：

```cpp
class CooperativeRouter {
public:
    RouteResult RouteFlow(const Flow& f,
                          const CoreOpticalTopology& topology,
                          const vector<int>& community_id,
                          const DenseMatrix& schedule_gain,
                          TopologyManager& topology_manager,
                          OpticalLinkStateManager& link_state,
                          WaitQueue& wait_queue,
                          const RoutingConfig& config);

    vector<OpticalPath> BuildDirectCandidates(NodeId s, NodeId d,
                                              const CoreOpticalTopology& topology) const;

    vector<OpticalPath> BuildStructureRelatedCandidates(NodeId s, NodeId d,
                                                        const CoreOpticalTopology& topology,
                                                        const vector<int>& community_id) const;

    vector<OpticalPath> BuildReachableCandidates(NodeId s, NodeId d,
                                                 const CoreOpticalTopology& topology,
                                                 uint32_t h_max) const;

    optional<OpticalPath> SelectBestPath(const vector<OpticalPath>& candidates,
                                         Rate r_f,
                                         const DenseMatrix& schedule_gain,
                                         const OpticalLinkStateManager& link_state) const;
};
```

### 5.8 WaitQueueManager

职责：

- 在光链路释放、拓扑更新、周期性定时器触发时重试等待流；
- 限制单次重试批大小；
- 防止无限高频重试导致仿真事件爆炸。

接口：

```cpp
class WaitQueueManager {
public:
    void OnLinkReleased(const vector<EdgeKey>& released_edges);
    void OnTopologyUpdated(uint64_t new_epoch);
    void OnPeriodicRetry();

    vector<RouteResult> RetryWaitingFlows(RetryReason reason,
                                          uint32_t max_batch);
};
```

---

## 6. 核心公式的工程实现

### 6.1 构造有向矩阵 W

输入：统计窗口内流集合 `F_window`。

```pseudo
function BuildDirectedMatrix(F_window, N, mapping):
    W = zeros(N, N)
    for f in F_window:
        v_s = mapping.GetOpticalNode(f.src_server)
        v_d = mapping.GetOpticalNode(f.dst_server)
        if v_s == v_d:
            continue
        W[v_s][v_d] += f.size_bytes
    for i in 0..N-1:
        W[i][i] = 0
    return W
```

### 6.2 构造无向矩阵 A

```pseudo
function BuildUndirectedMatrix(W):
    N = W.n
    A = zeros(N, N)
    for i in 0..N-1:
        for j in i+1..N-1:
            A[i][j] = W[i][j] + W[j][i]
            A[j][i] = A[i][j]
    return A
```

### 6.3 节点吞吐度与有效总流量

```pseudo
function ComputeWeightedDegreeAndM(A):
    N = A.n
    d = zeros(N)
    for i in 0..N-1:
        for j in 0..N-1:
            d[i] += A[i][j]
    M = 0.5 * sum(d)
    return d, M
```

异常处理：

```text
if M <= eps:
    无跨组观测流量。
    处理方式：
        community_id[i] = i
        S = zeros
        G = zeros
        E_o(t) = empty
    不应除以 0。
```

### 6.4 随机图零模型 P

```pseudo
P[i][j] = d[i] * d[j] / (2 * M)
```

注意：

- 只在 `M > eps` 时计算。
- 对角线可置 0。

### 6.5 模块度增益 B

```pseudo
B[i][j] = A[i][j] - eta * d[i] * d[j] / (2 * M)
```

其中 `eta > 0`。

### 6.6 结构性通信强度 S

```pseudo
S[i][j] = max(B[i][j], 0)
```

工程建议：

```pseudo
if S[i][j] < gain_epsilon:
    S[i][j] = 0
```

### 6.7 Louvain 社区划分

最小可实现版本可以调用已有 Louvain 库，但必须满足：

- 输入权重使用 `A` 或 `B` 的方式要一致。
- 论文目标是基于 `B` 的模块度目标。
- 工程中若库不支持负权，建议采用自定义局部移动，或只用 `A` 建图但移动收益按 `B` 公式计算。

推荐实现：自定义轻量 Louvain-style 局部移动。

节点加入社区收益：

```text
DeltaQ(i -> C) proportional to
    sum_{j in C} A[i][j] - eta * d[i] * SigmaTot(C) / (2*M)
```

伪代码：

```pseudo
function LouvainStyle(A, d, M, eta, max_iter, min_delta):
    N = A.n
    community[i] = i for each i
    SigmaTot[c] = d[i] for singleton community c

    if M <= eps:
        return community

    for iter in 1..max_iter:
        moved = false
        for i in NodeOrder(0..N-1):
            old_c = community[i]
            best_c = old_c
            best_gain = 0

            candidate_communities = communities of neighbors of i plus old_c

            // temporarily remove i from old community
            SigmaTot[old_c] -= d[i]

            for c in candidate_communities:
                d_i_C = sum(A[i][j] for j where community[j] == c)
                gain = d_i_C - eta * d[i] * SigmaTot[c] / (2*M)
                if gain > best_gain + min_delta:
                    best_gain = gain
                    best_c = c

            community[i] = best_c
            SigmaTot[best_c] += d[i]

            if best_c != old_c:
                moved = true

        if not moved:
            break

    return RenumberCommunities(community)
```

说明：

- 这不是完整多层 Louvain 压缩版本，但足够作为第一版工程实现。
- 若需要更贴近论文，可在局部移动收敛后执行社区压缩，并在聚合图上重复。
- 为保证结果可复现，默认 `NodeOrder` 使用固定顺序；如需随机顺序，必须设置随机种子。

### 6.8 社区影响函数 h

```pseudo
function CommunityFactor(c_i, c_j, alpha):
    if c_i == c_j:
        return 1.0
    else:
        return alpha
```

### 6.9 光路调度增益 G

```pseudo
G[i][j] = S[i][j] * CommunityFactor(community[i], community[j], alpha)
```

对角线强制置 0，且保持对称。

---

## 7. TL-OCS 调度规格

### 7.1 输入

```cpp
Schedule(
    epoch: uint64_t,
    A_prev: DenseMatrix,
    optical_nodes: vector<NodeId>,
    topology: TopologyManager,
    config: SchedulerConfig,
    routing_config: RoutingConfig
) -> SchedulerOutput
```

输入约束：

- `A_prev.n == optical_nodes.size()`；
- `A_prev` 必须近似对称；
- `A_prev[i][i] == 0`；
- `A_prev[i][j] >= 0`。

若输入不满足：

```text
strict_validation=true：返回调度失败。
strict_validation=false：进行修复：
    A[i][j] = max(0, 0.5*(A[i][j]+A[j][i]))
    A[i][i] = 0
```

### 7.2 输出

```cpp
SchedulerOutput {
    topology = G_o(t)
    x = x_ij^(t)
    community_id = c_i
    schedule_gain = G_ij^(t-1)
    structural_strength = S_ij^(t-1)
    optical_degree = deg_o(v_i)
}
```

### 7.3 候选光路集合

V2 候选光路集合：

```text
Omega(t) = {
    (v_i, v_j) |
    i < j,
    group(v_i) != group(v_j),
    G_ij^(t-1) > 0
}
```

工程实现：

```pseudo
function BuildCandidates(G, optical_nodes, topology, config):
    candidates = []
    for i in 0..N-1:
        for j in i+1..N-1:
            if config.require_cross_group:
                if topology.GetGroupOfOpticalNode(i) == topology.GetGroupOfOpticalNode(j):
                    continue
            if G[i][j] <= gain_epsilon:
                continue
            candidates.push({edge=(i,j), gain=G[i][j]})
    return candidates
```

### 7.4 贪心选路规则

排序：

```text
按 gain 降序。
若 gain 相同，建议使用确定性 tie-break：
    1. min(i,j) 小者优先；
    2. max(i,j) 小者优先。
```

伪代码：

```pseudo
function GreedySelect(candidates, k, K_o):
    E = empty set
    x = zeros(N,N)
    deg = zeros(N)

    sort candidates by (-gain, i, j)

    for cand in candidates:
        i, j = cand.edge

        if deg[i] >= k or deg[j] >= k:
            continue

        if K_o is set and |E| >= K_o:
            break

        x[i][j] = 1
        x[j][i] = 1
        E.add(EdgeKey(i,j))
        deg[i] += 1
        deg[j] += 1

    return E, x, deg
```

### 7.5 容量和负载初始化

```pseudo
function BuildCoreTopology(E, G, C_o, routing_config):
    theta_o = ComputeTheta(C_o, routing_config)
    topology.edges = E
    for e in E:
        topology.link_state[e] = OpticalLinkState(
            capacity=C_o,
            threshold=theta_o,
            current_load=0,
            schedule_gain=G[e.a][e.b],
            active=true
        )
    topology.adjacency = BuildAdjacency(E)
    return topology
```

### 7.6 完整调度伪代码

```pseudo
function TL_OCS_Schedule(epoch, A_prev, optical_nodes, topology, sched_cfg, route_cfg):
    ValidateOrRepairMatrix(A_prev)

    E_o = empty
    x = zeros(N,N)
    deg = zeros(N)

    d, M = ComputeWeightedDegreeAndM(A_prev)

    if M <= eps or sched_cfg.k == 0:
        community = [0..N-1]
        S = zeros(N,N)
        G = zeros(N,N)
        core = BuildCoreTopology(E_o, G, sched_cfg.optical_capacity, route_cfg)
        return SchedulerOutput(epoch, core, x, community, G, S, deg)

    P = ComputeExpectedMatrix(d, M)
    B = ComputeModularityGain(A_prev, d, M, sched_cfg.eta)
    S = PositivePart(B)

    community = LouvainStyle(
        A=A_prev,
        d=d,
        M=M,
        eta=sched_cfg.eta,
        max_iter=sched_cfg.louvain_max_iter,
        min_delta=sched_cfg.louvain_min_delta_q
    )

    G = ComputeScheduleGain(S, community, sched_cfg.alpha)
    candidates = BuildCandidates(G, optical_nodes, topology, sched_cfg)

    E_o, x, deg = GreedySelect(
        candidates,
        sched_cfg.k,
        sched_cfg.max_optical_links
    )

    core = BuildCoreTopology(E_o, G, sched_cfg.optical_capacity, route_cfg)

    return SchedulerOutput(
        epoch=epoch,
        topology=core,
        x=x,
        community_id=community,
        schedule_gain=G,
        structural_strength=S,
        optical_degree=deg
    )
```

### 7.7 调度复杂度

单周期主要复杂度：

```text
O(I_L * |E_f| + |Omega| log |Omega|)
```

其中：

- `I_L`：Louvain 迭代轮数；
- `|E_f|`：通信加权图中非零边数量；
- `|Omega|`：候选光路数量。

---

## 8. 协同路由规格

### 8.1 输入

```cpp
RouteFlow(
    f: Flow,
    topology: CoreOpticalTopology,
    community_id: vector<int>,
    schedule_gain: DenseMatrix,
    topology_manager: TopologyManager,
    link_state: OpticalLinkStateManager,
    wait_queue: WaitQueue,
    config: RoutingConfig
) -> RouteResult
```

### 8.2 输出

输出二选一：

1. `success=true`：返回端到端路径，并已完成光链路负载预留；
2. `queued=true`：跨组流暂无可行核心光路径，进入等待队列；
3. `success=false && queued=false`：输入异常或策略禁止等待/回退。

### 8.3 路由分类

```pseudo
v_s = g(f.src_server)
v_d = g(f.dst_server)

if v_s == v_d:
    path = path_e(h_s, h_d)
    return INTRA_ELECTRICAL
else:
    try DIRECT_OPTICAL
    try STRUCTURE_RELATED_INDIRECT_OPTICAL
    try REACHABLE_OPTICAL
    else WAIT_RETRY
```

### 8.4 直接光路径

候选条件：

```text
(v_s, v_d) in E_o(t)
```

伪代码：

```pseudo
function BuildDirectCandidates(s, d, topology):
    if EdgeKey(s,d) in topology.edges:
        return [OpticalPath(nodes=[s,d], type=DIRECT_OPTICAL)]
    return []
```

### 8.5 结构相关间接光路径

候选条件：

```text
(v_s, v_r) in E_o(t)
(v_r, v_d) in E_o(t)
community[v_r] == community[v_s] or community[v_r] == community[v_d]
```

伪代码：

```pseudo
function BuildStructureRelatedCandidates(s, d, topology, community):
    candidates = []
    neighbors_s = topology.adjacency[s]
    neighbors_d = topology.adjacency[d]
    relay_set = intersection(neighbors_s, neighbors_d)

    for r in relay_set:
        if community[r] == community[s] or community[r] == community[d]:
            candidates.push(OpticalPath(nodes=[s,r,d], type=STRUCTURE_RELATED_INDIRECT_OPTICAL))

    return candidates
```

复杂度：

```text
节点度受 k 限制时，约 O(k^2) 或 O(k) with hash set。
```

### 8.6 一般可达光路径

候选条件：

```text
p 是 G_o(t) 中从 v_s 到 v_d 的路径
hop_count(p) <= H_max
```

推荐最小实现：使用 BFS 找最短路径，而不是枚举所有路径。

伪代码：

```pseudo
function BuildReachableCandidates(s, d, topology, H_max):
    path = BFSShortestPath(topology.adjacency, s, d, max_depth=H_max)
    if path exists:
        return [OpticalPath(nodes=path, type=REACHABLE_OPTICAL)]
    return []
```

增强实现：枚举前 `K_paths` 条简单路径，然后按路径适配度排序。

```cpp
uint32_t k_reachable_paths = 8; // 工程默认建议
```

### 8.7 负载可行性

路径 `p` 对流 `f` 可行当且仅当：

```text
for every edge e in p:
    L_e^o(t) + r_f <= Theta_o
```

伪代码：

```pseudo
function IsFeasible(p, r_f, link_state):
    for e in p.edges:
        state = link_state.Get(e)
        if not state.active:
            return false
        if state.current_load + r_f > state.threshold + eps:
            return false
    return true
```

### 8.8 路径适配度排序

V2 使用词典序规则：

```text
负载可行过滤
 -> 路径类型优先级
 -> min H(p)
 -> max Gamma(p)
 -> min L(p)
```

其中：

```text
H(p) = |p.nodes| - 1
Gamma(p) = average(G_ij on edges of p)
L(p) = max(current_load on edges of p)
```

路径类型优先级：

```text
DIRECT_OPTICAL = 0
STRUCTURE_RELATED_INDIRECT_OPTICAL = 1
REACHABLE_OPTICAL = 2
```

伪代码：

```pseudo
function AnnotatePath(p, schedule_gain, link_state):
    p.hop_count = len(p.nodes) - 1
    p.structure_score = average(schedule_gain[e.a][e.b] for e in p.edges)
    p.max_link_load = max(link_state.GetLoad(e) for e in p.edges)
    return p

function SelectBestPath(candidates, r_f, schedule_gain, link_state):
    feasible = []
    for p in candidates:
        if IsFeasible(p, r_f, link_state):
            feasible.push(AnnotatePath(p, schedule_gain, link_state))

    if feasible.empty():
        return null

    sort feasible by (
        TypePriority(p.type) ascending,
        p.hop_count ascending,
        p.structure_score descending,
        p.max_link_load ascending,
        p.nodes lexicographic ascending  // deterministic tie-break
    )

    return feasible[0]
```

### 8.9 完整路由伪代码

```pseudo
function CooperativeRouteFlow(f):
    if f.size_bytes <= 0:
        return Failed(INVALID_FLOW_SIZE)

    if f.rate_estimate <= 0:
        if infer_rate_if_missing:
            f.rate_estimate = InferRate(f)
        else:
            return Failed(INVALID_RATE_ESTIMATE)

    v_s = topology_manager.GetOpticalNode(f.src_server)
    v_d = topology_manager.GetOpticalNode(f.dst_server)

    if v_s == INVALID or v_d == INVALID:
        return Failed(INVALID_SERVER_MAPPING)

    if v_s == v_d:
        p_e = topology_manager.GetIntraElectricalPath(f.src_server, f.dst_server)
        return RouteResult(
            success=true,
            queued=false,
            type=INTRA_ELECTRICAL,
            path=p_e
        )

    all_candidates = []

    // 1. direct optical
    direct = BuildDirectCandidates(v_s, v_d, core_topology)
    best = SelectBestPath(direct, f.rate_estimate, schedule_gain, link_state)
    if best != null:
        return BindAndReturn(f, best)

    // 2. structure-related two-hop optical
    related = BuildStructureRelatedCandidates(v_s, v_d, core_topology, community_id)
    best = SelectBestPath(related, f.rate_estimate, schedule_gain, link_state)
    if best != null:
        return BindAndReturn(f, best)

    // 3. reachable optical
    reachable = BuildReachableCandidates(v_s, v_d, core_topology, h_max)
    best = SelectBestPath(reachable, f.rate_estimate, schedule_gain, link_state)
    if best != null:
        return BindAndReturn(f, best)

    // 4. no optical path feasible
    if config.enable_wait_queue:
        wait_queue.Enqueue(f, "NO_FEASIBLE_CORE_OPTICAL_PATH")
        return RouteResult(success=false, queued=true, type=WAIT_RETRY)

    if config.fallback_to_electrical_for_cross_group:
        p = topology_manager.GetCrossGroupElectricalFallbackPath(f.src_server, f.dst_server)
        return RouteResult(success=true, queued=false, type=INTRA_ELECTRICAL, path=p)

    return Failed(NO_FEASIBLE_PATH)
```

### 8.10 BindAndReturn

```pseudo
function BindAndReturn(f, optical_path):
    // Two-phase reserve
    if not link_state.IsPathFeasible(optical_path, f.rate_estimate):
        return Failed(PATH_BECAME_INFEASIBLE)

    ok = link_state.ReservePath(f.id, optical_path, f.rate_estimate)
    if not ok:
        return Failed(RESERVE_FAILED)

    prefix = topology_manager.GetAccessPath(f.src_server, optical_path.nodes.front())
    suffix = topology_manager.GetAccessPath(optical_path.nodes.back(), f.dst_server)

    e2e = EndToEndPath(prefix, optical_path, suffix)
    return RouteResult(success=true, queued=false, type=optical_path.type, path=e2e)
```

---

## 9. 等待队列与事件机制

### 9.1 入队条件

跨组流满足以下任一条件时进入等待队列：

- 当前源宿无直接光路；
- 有直接光路但负载阈值不可行；
- 无结构相关两跳路径；
- 有两跳路径但负载阈值不可行；
- 无一般可达路径；
- 有一般可达路径但负载阈值不可行。

组内流不得进入等待队列。

### 9.2 重试触发事件

V2 要求等待队列在以下事件触发重试：

```text
1. 已有流完成并释放光链路负载；
2. 下一调度周期开始，TL-OCS 生成新核心光拓扑；
3. 控制器周期性触发等待队列重试。
```

工程实现：

```pseudo
on FlowCompleted(flow_id):
    released_edges = link_state.ReleasePath(flow_id)
    wait_queue_manager.OnLinkReleased(released_edges)

on TopologyUpdated(new_topology):
    topology_manager.ApplyOpticalTopology(new_topology)
    wait_queue_manager.OnTopologyUpdated(new_topology.epoch)

on RetryTimer():
    wait_queue_manager.OnPeriodicRetry()
```

### 9.3 重试策略

```pseudo
function RetryWaitingFlows(reason, max_batch):
    flows = wait_queue.PopRetryBatch(reason, max_batch)
    for f in flows:
        result = CooperativeRouteFlow(f)
        if result.success:
            wait_queue.Remove(f.id)
        else if result.queued:
            // already re-enqueued or kept
            continue
        else:
            if should_drop_on_failure:
                wait_queue.Remove(f.id)
            else:
                wait_queue.Enqueue(f, result.reason)
```

建议：

- 周期性重试不要一次扫描全部队列，避免事件风暴。
- 链路释放触发重试时，可以优先重试曾因该边拥塞失败的流；最小实现可直接取队头 batch。
- 若启用 `max_retry_count`，达到上限后返回 `FAILED_MAX_RETRY` 或 fallback 到电路径。

---

## 10. 拓扑更新期间的流状态处理

### 10.1 基本策略

V7 说明“路径一旦确定，在该数据流传输期间保持绑定；数据流完成后释放其占用的光链路负载估计”。因此工程默认：

```text
已绑定流不因新调度周期而改路。
新到达流使用最新 epoch 的核心光拓扑。
等待流在新拓扑生成后重试。
```

### 10.2 实现要求

需要区分：

```cpp
struct ActiveFlowBinding {
    Flow flow;
    uint64_t topology_epoch;
    OpticalPath reserved_path;
    Rate reserved_rate;
};
```

当拓扑更新时，有两种实现方式：

#### 方案 A：仿真简化实现，推荐第一版采用

- 新拓扑替换旧拓扑；
- 已有流仍保留其 `reserved_path` 的负载状态，直到完成；
- 如果新拓扑不包含旧路径边，旧边以 `active=false, reserved_by_existing_flow=true` 形式保留在 `OpticalLinkStateManager` 中，仅用于释放，不允许新流使用。

#### 方案 B：严格光路重构实现

- 拓扑更新前必须等待旧光路流完成，或模拟中断/重路由；
- 实现复杂，第一版不建议采用。

本规格建议采用方案 A，保证“流路径绑定”语义，并降低仿真复杂度。

---

## 11. 异常与边界情况

### 11.1 输入矩阵异常

| 情况 | 处理 |
|---|---|
| `A` 非方阵 | 调度失败，返回 `INVALID_MATRIX_SHAPE`。 |
| `A` 维度与 `V_o` 不一致 | 调度失败。 |
| `A[i][j] < 0` | strict 模式失败；非 strict 模式置 0。 |
| `A` 非对称 | 非 strict 模式对称化。 |
| `A[i][i] != 0` | 强制置 0。 |
| `M=0` | 输出空光拓扑和单节点社区。 |

### 11.2 拓扑异常

| 情况 | 处理 |
|---|---|
| `k=0` | 不建立任何光路。 |
| `N<2` | 不建立任何光路。 |
| 光接入节点缺少 group | 不参与候选光路，记录 warning。 |
| 候选集合为空 | 输出空核心光拓扑。 |
| `K_o` 小于可选边数 | 贪心达到上限后停止。 |

### 11.3 路由异常

| 情况 | 返回状态 |
|---|---|
| 无效服务器 ID | `INVALID_SERVER_MAPPING` |
| 组内电路径不存在 | `NO_INTRA_ELECTRICAL_PATH` |
| 跨组接入路径不存在 | `NO_ACCESS_PATH` |
| 无核心光路径 | `WAIT_RETRY` 或 `NO_FEASIBLE_PATH` |
| 有路径但负载不可行 | `WAIT_RETRY` |
| 预留时被并发事件占用 | 重新选择一次；仍失败则入队 |
| 释放未知 flow | warning，不影响仿真继续 |

### 11.4 负载异常

| 情况 | 处理 |
|---|---|
| `current_load + r_f > theta_o` | 不允许选择该路径。 |
| `theta_o > C_o` | 初始化时 clamp 到 `C_o`。 |
| 释放后负载 < 0 | 若绝对值小于 eps，置 0；否则记录 error。 |
| `r_f > theta_o` | 单流无法进入任何光路径；入队或 fallback/drop。 |

---

## 12. 仿真实验接口

### 12.1 流量场景生成器

```cpp
class TrafficGenerator {
public:
    vector<Flow> GenerateUniformBackground(const ExperimentConfig& cfg);
    vector<Flow> GenerateCommunityTraining(const ExperimentConfig& cfg);
    vector<Flow> GenerateAggregationTraining(const ExperimentConfig& cfg);
};
```

场景要求：

1. `uniform_background`：源宿服务器近似均匀选择，到达过程可用泊松过程。
2. `community_training`：同一训练通信社区内部的跨组通信概率更高。
3. `aggregation_training`：少量聚合组与其他工作组周期性通信。

### 12.2 对比策略

V2 仿真至少实现以下策略：

| 策略名 | 调度 | 路由 |
|---|---|---|
| `ElectricalOnly` | 不生成核心光路 | 全部走电网络。 |
| `StaticOptical` | 初始固定光拓扑 | 当前拓扑可达路径。 |
| `VolumeOCS` | 按 `A_ij` 降序选光路 | 最短路径或负载感知。 |
| `TLOCS_ShortestPath` | 按 `G_ij` 选光路 | 普通最短光路径。 |
| `TLHOC` | 按 `G_ij` 选光路 | V2 路径适配度协同路由。 |

### 12.3 指标采集

```cpp
struct Metrics {
    double avg_throughput;
    double avg_receiver_throughput;
    double avg_link_utilization;
    double avg_optical_hop_count;
    double direct_optical_ratio;
    double related_indirect_ratio;
    double reachable_optical_ratio;
    double wait_queue_ratio;
    double avg_wait_time;
    double scheduler_runtime_ms;
    double route_runtime_us;
};
```

V7 明确给出的核心指标：

- 平均吞吐量；
- 平均接收端吞吐；
- 平均链路利用率。

工程建议额外采集：

- 三类路径命中比例；
- 等待队列长度和平均等待时间；
- 调度时间和单流路由时间；
- 光链路负载分布；
- 不同流量负载 `rho` 下的性能曲线。

---

## 13. 控制器主循环

### 13.1 初始化

```pseudo
function Initialize():
    LoadExperimentConfig()
    BuildServersAndGroups()
    BuildIntraElectricalTopology()
    BuildOpticalAccessNodes()
    InitializeTrafficObserver()
    InitializeWaitQueue()

    A0 = WarmupOrProfileMatrix()
    sched_out = TL_OCS_Schedule(epoch=0, A0, ...)
    ApplySchedulerOutput(sched_out)
```

### 13.2 运行循环

```pseudo
for event in simulation_events:
    now = event.time

    if event.type == FLOW_ARRIVAL:
        f = event.flow
        traffic_observer.OnFlowObserved(f, mapping)
        result = cooperative_router.RouteFlow(f, current_topology, ...)
        metrics.OnRouteResult(result)

    if event.type == FLOW_COMPLETED:
        released_edges = link_state.ReleasePath(event.flow_id)
        metrics.OnFlowCompleted(event.flow_id)
        wait_queue_manager.OnLinkReleased(released_edges)

    if event.type == STAT_WINDOW_END:
        W = traffic_observer.GetDirectedMatrixAndReset()
        A = traffic_observer.BuildUndirectedMatrix(W)
        last_A = A

    if event.type == SCHEDULING_PERIOD_END:
        sched_out = TL_OCS_Schedule(epoch+1, last_A, ...)
        ApplySchedulerOutput(sched_out)
        wait_queue_manager.OnTopologyUpdated(epoch+1)

    if event.type == WAIT_QUEUE_RETRY_TIMER:
        wait_queue_manager.OnPeriodicRetry()
```

### 13.3 调度周期与统计窗口

V7 允许：

```text
T_o = tau
```

或：

```text
T_o = m * tau
```

工程实现应支持两者。

```cpp
struct ControlTimingConfig {
    TimeNs stat_window_tau;
    TimeNs optical_schedule_period_To;
};
```

若 `T_o` 是多个统计窗口，可选择：

```text
A_for_schedule = sum of A over windows in current schedule period
```

---

## 14. 测试与验收标准

### 14.1 单元测试

#### Matrix 构造

- 给定两个方向流量，检查 `A[i][j] = W[i][j] + W[j][i]`。
- 检查对角线为 0。
- 检查非对称 `W` 可以生成对称 `A`。

#### 结构性通信强度

- 构造简单 3 节点矩阵，手算 `d_i`、`M`、`B`、`S`。
- 检查 `S=max(B,0)`。
- 检查 `eta` 增大时 `S` 不应增加。

#### 调度器

- `k=0` 输出空拓扑。
- `k=1` 时每个节点度不超过 1。
- 候选边按 `G` 降序选择。
- `K_o=1` 时最多输出一条光路。
- `require_cross_group=true` 时同组节点对不被选中。

#### 路由器

- 组内流返回 `INTRA_ELECTRICAL`。
- 有直连光路且负载可行时返回 `DIRECT_OPTICAL`。
- 无直连但有社区相关两跳路径时返回 `STRUCTURE_RELATED_INDIRECT_OPTICAL`。
- 只有一般可达路径时返回 `REACHABLE_OPTICAL`。
- 全部不可行时进入等待队列。
- 同类型同跳数路径选择 `Gamma(p)` 更大的路径。
- `Gamma` 相同选择最大链路负载更低的路径。

#### 负载状态

- 绑定路径后每条边负载增加 `r_f`。
- 流完成后每条边负载减少 `r_f`。
- 负载超过阈值的路径不可选。
- 释放未知流不会导致崩溃。

### 14.2 集成测试

#### 测试 1：社区型流量

期望：

- TL-OCS 选中的光路主要落在高 `S_ij` / 高 `G_ij` 边上；
- TL-HOC 的直接光路径或结构相关路径比例高于 `TLOCS_ShortestPath`；
- 等待队列不应持续单调增长。

#### 测试 2：聚合型流量

期望：

- 聚合节点相关边应获得较高候选排序，但随机零模型会抑制纯高吞吐偏置；
- TL-HOC 应能通过结构相关间接光路径利用核心拓扑。

#### 测试 3：高负载 rho=0.9

期望：

- 光链路负载不超过 `Theta_o`；
- 等待队列可能增长，但不出现负载越界；
- 仿真不会因重试事件爆炸而卡死。

### 14.3 可复现性要求

- 所有随机过程必须使用显式 seed。
- Louvain 节点遍历顺序默认固定。
- 候选边排序必须有确定性 tie-break。
- 输出每个调度周期的：
  - `A`；
  - `S`；
  - `community_id`；
  - `G`；
  - `E_o(t)`；
  - 每条光路负载时间序列。

---

## 15. 建议文件组织

如果当前仿真工程是 C++ / NS-3，可按以下目录组织：

```text
src/tlhoc/
├── model/
│   ├── tlhoc-types.h
│   ├── traffic-matrix.h
│   ├── optical-topology.h
│   └── flow-record.h
├── control/
│   ├── traffic-observer.h/.cc
│   ├── structure-analyzer.h/.cc
│   ├── tl-ocs-scheduler.h/.cc
│   ├── cooperative-router.h/.cc
│   ├── wait-queue.h/.cc
│   └── tlhoc-controller.h/.cc
├── experiment/
│   ├── traffic-generator.h/.cc
│   ├── baseline-strategies.h/.cc
│   └── metrics-collector.h/.cc
└── tests/
    ├── test-traffic-matrix.cc
    ├── test-tl-ocs-scheduler.cc
    ├── test-cooperative-router.cc
    └── test-wait-queue.cc
```

若当前工程是 Python 原型，可保持同样模块边界：

```text
tlhoc/
├── types.py
├── traffic_observer.py
├── structure_analyzer.py
├── tl_ocs_scheduler.py
├── cooperative_router.py
├── wait_queue.py
├── controller.py
├── traffic_generator.py
└── metrics.py
```

---

## 16. Codex 实现优先级

### P0：必须完成，否则 V2 不成立

1. 实现 `W -> A -> S -> community -> G -> E_o(t)`。
2. TL-OCS 按 `G_ij` 而非 `A_ij` 选光路。
3. 输出并保存 `community_id`、`schedule_gain`、`optical_load`。
4. 路由支持三类核心光路径：直接、结构相关两跳、一般可达。
5. 路由按负载可行性过滤，并更新/释放光链路负载。
6. 无可行跨组光路径时进入等待队列。

### P1：建议完成，用于稳定实验

1. 支持 `H_max` 限制的一般路径搜索。
2. 支持周期性等待队列重试。
3. 支持拓扑更新后等待流重试。
4. 支持 5 类对比策略。
5. 输出路径类型比例和等待队列指标。

### P2：增强项

1. 完整多层 Louvain 聚合。
2. K-shortest reachable optical paths。
3. 拓扑更新时对旧光路 active/inactive 状态进行严格建模。
4. 支持每组多个光接入节点。
5. 支持 deadline-aware waiting/drop 策略。

---

## 17. 最小可运行 TL-HOC 伪代码总览

```pseudo
// Controller initialization
A0 = BuildInitialMatrixByWarmupOrProfile()
scheduler_output = TL_OCS_Schedule(epoch=0, A0, ...)
current_topology = scheduler_output.topology
community_id = scheduler_output.community_id
schedule_gain = scheduler_output.schedule_gain

// Event loop
on FlowArrival(f):
    observer.OnFlowObserved(f)
    result = CooperativeRouteFlow(
        f,
        current_topology,
        community_id,
        schedule_gain,
        link_state,
        wait_queue
    )
    metrics.RecordRoute(result)

on FlowCompleted(flow_id):
    released_edges = link_state.ReleasePath(flow_id)
    wait_queue_manager.RetryWaitingFlows(LINK_RELEASED)

on StatWindowEnd():
    W = observer.GetDirectedMatrixAndReset()
    A_last = BuildUndirectedMatrix(W)

on SchedulePeriodEnd():
    scheduler_output = TL_OCS_Schedule(epoch+1, A_last, ...)
    current_topology = scheduler_output.topology
    community_id = scheduler_output.community_id
    schedule_gain = scheduler_output.schedule_gain
    topology_manager.ApplyOpticalTopology(current_topology)
    wait_queue_manager.RetryWaitingFlows(TOPOLOGY_UPDATED)

on RetryTimer():
    wait_queue_manager.RetryWaitingFlows(PERIODIC_RETRY)
```

---

## 18. 实现时最容易出错的点

1. **把 `S` 和 `G` 混用**：`S=max(B,0)` 是结构性通信强度；`G=S*h(c_i,c_j)` 是最终调度排序收益。
2. **路由阶段丢失社区标签**：V2 要求路由使用 `community_id` 判断结构相关中继。
3. **只实现直连光路命中**：这会退化回 V1，必须实现两跳结构相关路径和一般可达路径。
4. **无可行路径时直接走 EPS**：V2 默认是等待队列；是否 fallback 到电路径必须作为可配置 baseline，不应作为 TL-HOC 默认行为。
5. **拓扑更新时清空已有流负载**：路径绑定流在完成前仍占用资源，不能因为新周期直接消失。
6. **`Theta_o` 大于 `C_o`**：必须 clamp 或报错。
7. **候选边没有排除同组节点**：V2 的核心光路候选强调跨组节点对，主实验每组一个光接入节点时这点尤其明确。
8. **矩阵除以零**：`M=0` 时不能计算 `P` 或 `B`。
9. **无向边状态重复**：所有光路负载必须使用规范化 `EdgeKey(min,max)`。
10. **随机性不可复现**：Louvain、流量生成、候选路径 tie-break 都要固定 seed 或固定顺序。

---

## 19. 交付物检查清单

Codex 完成后，工程中至少应存在：

- [ ] `StructureAnalyzer::Analyze()`
- [ ] `TLOcsScheduler::Schedule()`
- [ ] `CooperativeRouter::RouteFlow()`
- [ ] `BuildDirectCandidates()`
- [ ] `BuildStructureRelatedCandidates()`
- [ ] `BuildReachableCandidates()`
- [ ] `SelectBestPath()`
- [ ] `OpticalLinkStateManager::ReservePath()`
- [ ] `OpticalLinkStateManager::ReleasePath()`
- [ ] `WaitQueue::Enqueue()` and retry logic
- [ ] `TLHocController` event loop integration
- [ ] Baselines: `ElectricalOnly`, `StaticOptical`, `VolumeOCS`, `TLOCS_ShortestPath`, `TLHOC`
- [ ] Metrics output CSV/JSON
- [ ] Unit tests and deterministic seed support

---

## 20. 推荐配置文件模板

```yaml
experiment:
  num_servers: 128
  num_groups: 8
  servers_per_group: 16
  num_optical_nodes: 8
  random_seed: 1
  rho_values: [0.3, 0.5, 0.7, 0.9]

links:
  electrical_capacity_bps: 10000000000      # 工程默认建议：10 Gbps
  optical_capacity_bps: 100000000000        # 工程默认建议：100 Gbps

control:
  stat_window_tau_ns: 1000000000            # 1s，工程默认建议
  optical_schedule_period_ns: 1000000000    # 1s，工程默认建议

scheduler:
  k: 2
  max_optical_links: null
  eta: 1.0
  alpha: 0.5
  louvain_max_iter: 20
  louvain_min_delta_q: 1.0e-9
  require_cross_group: true
  keep_existing_flows_on_old_path: true

routing:
  theta_ratio: 0.8
  optical_threshold_bps: null
  h_max: 3
  enable_wait_queue: true
  fallback_to_electrical_for_cross_group: false
  retry_interval_ns: 10000000               # 10ms，工程默认建议
  retry_max_batch: 1024
  max_retry_count: 0
  infer_rate_if_missing: true
  default_flow_duration_ns: 100000000

traffic:
  scenario: community_training
  arrival_process: poisson
  small_flow_bytes: 1000000
  large_flow_bytes: 100000000
  small_flow_probability: 0.8

metrics:
  output_dir: results/tlhoc
  dump_matrices: true
  dump_paths: true
  dump_link_load_timeseries: true
```
