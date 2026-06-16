# v1_algorithm_summary.md

# 第一版算法（V5）摘要：TL-OCS 光路调度与光电协同路由



## 1. 算法定位

第一版算法解决光电混合数据中心网络中的两个问题：

1. **OCS 光路调度**：控制器根据上一统计窗口观测到的接入节点间通信矩阵，选择当前周期应建立的 OCS 光链路集合。
2. **光电协同路由**：新 flow 到达后，根据当前 OCS 光链路集合判断是否走一跳 OCS 直连路径；未命中或容量不足时走 EPS 电层路径。

整体控制链路为：

```text
W^(t-1)
  -> A^(t-1)
  -> B^(t-1)
  -> community labels C^(t-1)
  -> G_ij^(t-1)
  -> E_o(t)
  -> path(f)
```

其中：

- `W^(t-1)`：上一统计窗口的有向流量矩阵。
- `A^(t-1)`：由 `W` 双向合并得到的无向通信强度矩阵。
- `B^(t-1)`：随机图零模型下的模块度增益矩阵。
- `C^(t-1)`：Louvain-style 社区划分结果。
- `G_ij^(t-1)`：候选光路调度增益。
- `E_o(t)`：当前 OCS 周期生效的光链路集合。
- `path(f)`：新 flow 的实际路径，取值为 OCS 直连路径或 EPS 基础路径。

---

## 2. 网络模型

### 2.1 物理拓扑

第一版采用扁平的光电混合 DCN 模型：

```text
G_p(t) = (V, E_e ∪ E_o(t))
```

含义：

- `V = {v_1, v_2, ..., v_N}`：接入节点或 ToR 节点集合。
- `E_e`：EPS 静态电层链路集合。
- `E_o(t)`：第 `t` 个 OCS 调度周期内生效的光链路集合。
- `C_e`：EPS 链路容量。
- `C_o`：OCS 光链路容量，通常 `C_o > C_e`。

第一版不区分服务器集合、组内电交换节点、光接入节点集合等分层对象。仿真实现中可以把服务器侧流量聚合到接入节点级别，然后在接入节点对之间进行光路调度。

### 2.2 OCS 光路变量

对任意接入节点对 `(v_i, v_j)`，定义二值变量：

```text
x_ij^(t) = 1  表示第 t 个周期内 v_i 与 v_j 之间存在 OCS 光路
x_ij^(t) = 0  表示不存在 OCS 光路
```

约束：

```text
x_ij^(t) = x_ji^(t)
x_ii^(t) = 0
```

当前周期光链路集合：

```text
E_o(t) = {(v_i, v_j) | x_ij^(t) = 1, i < j}
```

### 2.3 光端口约束

每个节点最多使用 `k` 个 OCS 光端口：

```text
sum_{j != i} x_ij^(t) <= k, for all v_i in V
```

实现时应维护：

```text
deg_o[v_i] = 当前周期节点 v_i 已经占用的 OCS 端口数量
```

候选边 `(i, j)` 只有在以下条件同时满足时才能被加入 `E_o(t)`：

```text
deg_o[i] < k
deg_o[j] < k
```

---

## 3. 流量观测与矩阵构造

### 3.1 有向流量矩阵 W

控制器以统计窗口 `τ` 为单位观测接入节点间通信量。第 `t` 个统计窗口得到：

```text
W^(t) = [w_ij^(t)]_{N x N}
```

其中：

- `w_ij^(t)`：统计窗口 `τ` 内从接入节点 `v_i` 到 `v_j` 的实际传输字节数。
- `w_ii^(t) = 0`。

工程实现建议：

- 在源 ToR 或源接入节点侧统计。
- 每个跨接入节点数据包只在源侧计数一次。
- 统计粒度为接入节点对，不是服务器对。

### 3.2 无向通信强度矩阵 A

由于 OCS 光路被建模为无向直连资源，第一版将双向流量合并：

```text
A_ij^(t) = w_ij^(t) + w_ji^(t), i != j
A_ii^(t) = 0
A_ij^(t) = A_ji^(t)
```

`A^(t)` 是 TL-OCS 的主要输入。

---

## 4. 结构指标计算

### 4.1 节点吞吐度 d

对无向通信强度矩阵 `A`，定义节点加权吞吐度：

```text
d_i^(t) = sum_j A_ij^(t)
```

`d_i` 表示节点 `v_i` 在当前窗口内参与的总通信量。

### 4.2 全网有效总流量 M

```text
M^(t) = 1/2 * sum_i sum_j A_ij^(t)
```

由于 `A` 是无向矩阵，矩阵求和会把每条无向边算两次，因此需要乘以 `1/2`。

实现注意：

- 若 `M == 0`，说明当前窗口无跨接入节点通信，应直接输出空光路集合或保持默认策略。
- 计算 `P_ij` 和 `B_ij` 时必须避免除零。

### 4.3 随机图零模型期望 P

第一版引入随机图零模型，用于修正高吞吐节点偏置：

```text
P_ij^(t) = d_i^(t) * d_j^(t) / (2 * M^(t))
```

含义：在保持每个节点总体吞吐规模不变的背景下，节点对 `(i, j)` 之间自然可能出现的期望通信强度。

### 4.4 模块度增益矩阵 B

```text
B_ij^(t) = A_ij^(t) - η * d_i^(t) * d_j^(t) / (2 * M^(t))
```

其中：

- `η > 0`：分辨率参数。
- `B_ij > 0`：实际通信强度高于随机背景期望，表示正向非均衡通信偏离。
- `B_ij <= 0`：不作为正向结构收益使用。

### 4.5 社区划分目标 Q

设 `c_i` 为节点 `v_i` 所属社区编号，定义：

```text
δ(c_i, c_j) = 1 if c_i == c_j else 0
```

模块度目标：

```text
Q(η) = 1 / (2M) * sum_i sum_j B_ij * δ(c_i, c_j)
```

Louvain-style 社区划分在 `B` 上优化该目标，用于识别训练流量中的高内聚通信结构。

### 4.6 光调度增益 G

先定义基础增益：

```text
G_base_ij = max(B_ij, 0)
```

再定义社区影响函数：

```text
h(c_i, c_j) = 1      if c_i == c_j
h(c_i, c_j) = α      if c_i != c_j
```

其中：

- `α ∈ (0, 1)`：跨社区折减系数。

最终光调度增益：

```text
G_ij = max(B_ij, 0) * h(c_i, c_j)
```

只有 `G_ij > 0` 的节点对进入候选光路集合。

---

## 5. TL-OCS 光路调度算法

### 5.1 输入

```text
A^(t-1)    上一统计窗口的无向通信强度矩阵
k          单节点 OCS 光端口上限
η          随机图零模型分辨率参数
α          跨社区折减系数
```

可选工程参数：

```text
θ_f        流量边筛选阈值，用于减少 Louvain 图规模
max_iter   Louvain 最大迭代轮数
seed       节点遍历顺序随机种子
```

### 5.2 输出

```text
E_o(t)     当前周期 OCS 光链路集合
x_ij^(t)   当前周期光路建立变量
c_i        节点社区标签
G_ij       候选光路调度增益
```

第一版论文中调度阶段主要输出 `E_o(t)`。工程实现可以额外保留 `c_i` 与 `G_ij` 便于日志分析，但第一版路由不会使用它们做多跳路径选择。

### 5.3 优化目标

第一版将光路选择抽象为：

```text
maximize sum_{i<j} x_ij^(t) * G_ij^(t)
```

约束：

```text
sum_{j != i} x_ij^(t) <= k, for all i
x_ij^(t) ∈ {0, 1}
x_ij^(t) = x_ji^(t)
x_ii^(t) = 0
```

### 5.4 调度流程

```text
Algorithm TL-OCS-Schedule(A, k, η, α):
    1. 计算每个节点的吞吐度 d_i = sum_j A_ij
    2. 计算全网有效总流量 M = 0.5 * sum_i sum_j A_ij
    3. 若 M == 0，返回空 E_o 或默认 E_o
    4. 对每个 i < j：
           P_ij = d_i * d_j / (2M)
           B_ij = A_ij - η * P_ij
    5. 在 B 矩阵上执行 Louvain-style 社区划分，得到 c_i
    6. 对每个 i < j：
           base = max(B_ij, 0)
           h = 1 if c_i == c_j else α
           G_ij = base * h
    7. 构造候选集合 Ω = {(i,j) | i < j and G_ij > 0}
    8. 将 Ω 按 G_ij 降序排序
    9. 初始化 deg_o[i] = 0, E_o = empty
   10. 对排序后的每条候选边 (i,j)：
           if deg_o[i] < k and deg_o[j] < k:
               x_ij = 1
               add (i,j) to E_o
               deg_o[i] += 1
               deg_o[j] += 1
           else:
               skip
   11. 返回 E_o, x, c, G
```

### 5.5 Louvain-style 社区划分要点

初始状态：

```text
c_i = i, for all nodes
```

局部移动规则：

```text
对每个节点 v_i：
    枚举候选社区 C
    计算移动到 C 后的 ΔQ
    若存在最大正增益社区，则移动 v_i
    若所有 ΔQ <= 0，则保持原社区
```

节点加入社区的收益与下式相关：

```text
ΔQ_{i -> C} ∝ d_{i,C} - η * d_i * Σ_tot(C) / (2M)
```

其中：

```text
d_{i,C} = sum_{v_j in C} A_ij
Σ_tot(C) = sum_{v_j in C} d_j
```

一轮局部移动完成后，将社区压缩为超节点，并在聚合图上重复局部移动，直到模块度目标不再增加或达到最大迭代次数。

### 5.6 候选边排序与选边规则

候选集合：

```text
Ω = {(v_i, v_j) | i < j and G_ij > 0}
```

排序键：

```text
primary:   G_ij descending
secondary: A_ij descending       # 建议工程 tie-breaker
tertiary:  node id pair ascending # 保证可复现实验
```

选边硬约束：

```text
只有当 deg_o[i] < k 且 deg_o[j] < k 时，才能选择 (i, j)
```

---

## 6. 第一版光电协同路由算法

### 6.1 路由输入

```text
E_o(t)       当前周期 OCS 光链路集合
x_ij^(t)     当前周期光路建立状态
f            新到达 flow
v_s          flow 源主机映射到的接入节点
v_d          flow 目的主机映射到的接入节点
r_f          flow 的速率估计
Θ_o          单条 OCS 光路允许分配的速率阈值
L_ij^ocs(t)  当前周期 OCS 光路 (i,j) 上已分配的估计负载
```

### 6.2 路由输出

```text
path(f)       flow 的路径
path_type     OCS 或 EPS
z_f^ocs(t)    是否走 OCS 的二值变量
```

其中：

```text
z_f^ocs(t) = 1 表示 flow 被分配到 OCS
z_f^ocs(t) = 0 表示 flow 进入 EPS
```

### 6.3 OCS 命中规则

新 flow `f` 的源目的接入节点为：

```text
v_s = g(h_s)
v_d = g(h_d)
```

若以下两个条件同时满足：

```text
x_sd^(t) == 1
L_sd^ocs(t) + r_f <= Θ_o
```

则：

```text
z_f^ocs(t) = 1
path(f) = path_ocs(v_s, v_d) = (v_s, v_d)
L_sd^ocs(t) += r_f
```

### 6.4 EPS 回退规则

若 OCS 不存在或容量不足：

```text
z_f^ocs(t) = 0
path(f) = path_eps(v_s, v_d)
```

其中 `path_eps(v_s, v_d)` 由 EPS 基础路由规则决定。第一版不规定复杂 EPS 路由策略，仿真工程中可使用已有最短路径、ECMP 或静态路由。

### 6.5 连接保持规则

已经启动的 flow 保持原路径直到完成：

```text
OCS 光路更新只影响后续新到达 flow
不对已启动 flow 重新执行应用层光路分配
```

当 OCS flow 完成或超时：

```text
L_sd^ocs(t) -= r_f
```

工程实现应确保负载不小于 0：

```text
L_sd^ocs(t) = max(0, L_sd^ocs(t) - r_f)
```

### 6.6 路由流程伪代码

```text
Algorithm Route-Flow-V1(f, E_o, Θ_o):
    v_s = g(f.src_host)
    v_d = g(f.dst_host)
    r_f = estimate_rate(f)

    if v_s == v_d:
        path_type = EPS
        path = path_eps(v_s, v_d)
        return path, path_type

    if x[v_s][v_d] == 1 and L_ocs[v_s][v_d] + r_f <= Θ_o:
        z_f_ocs = 1
        path_type = OCS
        path = (v_s, v_d)
        L_ocs[v_s][v_d] += r_f
    else:
        z_f_ocs = 0
        path_type = EPS
        path = path_eps(v_s, v_d)

    record_flow_assignment(f, path_type, path)
    return path, path_type
```

---

## 7. 控制周期与状态更新

### 7.1 时间尺度

```text
τ      统计窗口长度
T_o    OCS 光路调度周期
```

一般关系：

```text
T_o >= τ
```

一个 OCS 周期可以包含一个或多个统计窗口。

### 7.2 周期流程

```text
At each statistics window:
    1. 收集数据包或 flow 级统计，更新 W^(t)
    2. 在窗口结束时构造 A^(t)

At each OCS scheduling boundary:
    1. 使用已经完成的历史窗口 A^(t-1)
    2. 运行 TL-OCS-Schedule
    3. 得到 E_o(t)
    4. 更新 OCS 光层连接状态
    5. 更新数据面高优先级 OCS 匹配规则
    6. EPS 默认规则保持全网连通性

For each new flow arrival:
    1. 执行 Route-Flow-V1
    2. 命中 OCS 且容量可用则走一跳光路
    3. 否则走 EPS
```

### 7.3 数据面规则优先级

推荐实现：

```text
Priority 1: OCS direct-rule for active optical link (v_i, v_j)
Priority 2: EPS default forwarding rule
```

匹配逻辑：

```text
先匹配源目的接入节点对是否为当前 OCS 光路
再匹配 EPS 默认路径
```

---

## 8. 主要参数

| 参数    | 含义                 | 所属模块     |
| ----- | ------------------ | -------- |
| `N`   | 接入节点数量             | 拓扑       |
| `C_e` | EPS 链路容量           | 数据面      |
| `C_o` | OCS 光链路容量          | 数据面      |
| `k`   | 单节点 OCS 光端口上限      | 光路调度约束   |
| `τ`   | 统计窗口长度             | 控制周期     |
| `T_o` | OCS 调度周期           | 控制周期     |
| `η`   | 随机图零模型分辨率参数        | 结构识别     |
| `α`   | 跨社区折减系数            | 光调度增益    |
| `Θ_o` | 单条 OCS 光路分配速率阈值    | 路由容量约束   |
| `θ_f` | 流量边筛选阈值，可选         | 控制器复杂度控制 |
| `I_L` | Louvain-style 迭代轮数 | 复杂度分析    |

---

## 9. 复杂度

设：

```text
|E_f(t-1)|    上一窗口流量关系边数量
|Ω^(t)|       候选光路数量
I_L           Louvain-style 社区划分迭代轮数
```

主要复杂度：

```text
计算 d、M、B:              O(|E_f(t-1)|)
Louvain-style 社区划分:    O(I_L * |E_f(t-1)|)
候选边排序:               O(|Ω^(t)| log |Ω^(t)|)
贪心选边:                 O(|Ω^(t)|)
```

因此单个 OCS 周期的主要复杂度：

```text
O(I_L * |E_f(t-1)| + |Ω^(t)| log |Ω^(t)|)
```

每个新 flow 的路由复杂度：

```text
O(1)  # 若 E_o(t) 使用哈希表或邻接矩阵维护
```

EPS 路径计算复杂度取决于仿真工程已有的电层路由实现。

---

## 10. 仿真实验设置摘要

### 10.1 拓扑

第一版实验基于 NS-3 构建光电混合 DCN：

- EPS 网络提供基础全连通能力。
- OCS 网络在接入节点之间提供动态直连光路。
- 每个接入节点连接若干服务器。
- 每个接入节点最多同时使用 `k` 个 OCS 光端口。

### 10.2 流量模式

第一版定义三类训练流量场景：

1. **均匀背景流量**
   
   - 源目的服务器近似均匀选择。
   - flow 到达服从泊松过程。
   - 用于模拟结构较弱的背景业务。

2. **社区内高通信流量**
   
   - 接入节点被划分为若干训练社区。
   - 同社区内部通信概率更高。
   - 用于验证 TL-OCS 对流量空间局部性的识别能力。

3. **参数聚合流量**
   
   - 少量接入节点作为 aggregator。
   - worker 周期性向 aggregator 发送 flow，并接收返回流。
   - 用于验证随机图零模型对高吞吐节点偏置的修正能力。

### 10.3 Flow 大小分布

第一版采用小流/大流混合分布：

```text
L_f = L_s with probability p_s
L_f = L_l with probability 1 - p_s
```

所有对比方案应使用完全相同的 flow 序列。

### 10.4 对比方案

| 方案           | 说明                                |
| ------------ | --------------------------------- |
| `EPS`        | 关闭 OCS，所有 flow 走 EPS。             |
| `OCS-Volume` | 启用 OCS，但按绝对通信强度 `A_ij` 排序选光路。     |
| `TL-OCS`     | 第一版算法，按随机图零模型修正后的结构收益 `G_ij` 选光路。 |

三类方案必须使用相同拓扑、相同 flow 序列、相同链路容量、相同端口数量和相同控制周期。

### 10.5 评价指标

| 指标           | 说明                      |
| ------------ | ----------------------- |
| 平均 flow 完成时间 | flow 从开始发送到完整接收的平均时长。   |
| 尾部 flow 完成时间 | 90% 与 95% 分位 FCT。       |
| 平均接收吞吐       | 成功接收字节数按仿真时间归一化。        |
| OCS flow 命中率 | 分配到 OCS 的 flow 数量占比。    |
| OCS 字节命中率    | 经 OCS 成功接收的字节占比。        |
| OCS 光链路利用率   | 已建立光路上的有效传输量与容量比例。      |
| EPS 平均链路利用率  | EPS 链路整体负载水平。           |
| EPS 最大链路利用率  | 电层瓶颈链路压力。               |
| 社区内光路比例      | 所选 OCS 光路中连接同一社区节点对的比例。 |

---

## 11. 工程模块建议

### 11.1 Controller / Scheduler

建议模块：

```text
TrafficMatrixCollector
TrafficMatrixAggregator
TL_OCS_Scheduler
LouvainCommunityDetector
OpticalTopologyManager
```

主要职责：

- 收集 `W`。
- 构造 `A`。
- 计算 `d`、`M`、`P`、`B`、`G`。
- 执行社区划分。
- 按端口约束选择 `E_o(t)`。
- 下发或更新 OCS 连接状态。

### 11.2 Routing / Flow Assignment

建议模块：

```text
HybridFlowRouterV1
OCSLoadTracker
EPSBaseRouter
FlowAssignmentLogger
```

主要职责：

- 对新 flow 映射 `v_s = g(h_s)`、`v_d = g(h_d)`。
- 查询 `x_sd^(t)`。
- 判断 `L_sd^ocs + r_f <= Θ_o`。
- 选择 OCS 直连路径或 EPS 路径。
- 记录 path type 与命中情况。
- flow 结束时释放 OCS 负载。

### 11.3 Logging

建议输出日志：

```text
per_period_topology.csv
    period_id, src_tor, dst_tor, G_ij, A_ij, B_ij, c_src, c_dst

per_flow_assignment.csv
    flow_id, start_time, finish_time, src_host, dst_host, src_tor, dst_tor,
    size_bytes, rate_est, path_type, ocs_hit, path

ocs_link_load.csv
    time, period_id, src_tor, dst_tor, load_rate, threshold, active_flow_count

metrics_summary.csv
    scheme, scenario, seed, avg_fct, p90_fct, p95_fct,
    ocs_flow_hit_rate, ocs_byte_hit_rate,
    avg_throughput, eps_avg_util, eps_max_util, ocs_util,
    intra_community_lightpath_ratio
```

---

## 12. 第一版实现边界

Codex 修改或复现第一版工程时，应避免把第二版功能混入第一版。第一版明确不包含以下机制：

1. 不包含 `TL-HOC` 总体框架命名。
2. 不包含分层拓扑 `V_s, V_e, V_o, E_intra^e, E_o(t)`。
3. 不包含核心光拓扑上的多跳路径搜索。
4. 不包含直接光路径、结构相关间接光路径、一般可达光路径的三级候选路径分类。
5. 不包含路径适配度函数。
6. 不包含等待队列 `Q(t)`。
7. 不包含链路释放后对等待队列重试。
8. 不包含路由阶段继续使用社区标签和 `G_ij` 进行路径排序。
9. 不包含组内电网络与跨组光核心的显式分层控制。
10. 不包含“无可行光路径则等待”的策略；第一版无可用 OCS 时直接回退 EPS。

第一版的路由控制原则可以概括为：

```text
Direct OCS hit and capacity feasible -> OCS one-hop path
Otherwise -> EPS fallback path
```

---

## 13. 最小验收测试

### 13.1 矩阵构造测试

输入：

```text
w_12 = 10, w_21 = 5
w_13 = 7,  w_31 = 0
```

期望：

```text
A_12 = 15
A_13 = 7
A_ii = 0
A is symmetric
```

### 13.2 端口约束测试

给定：

```text
k = 1
候选边按收益排序: (1,2), (1,3), (2,3)
```

若 `(1,2)` 被选中，则节点 1 和 2 端口已满，后续 `(1,3)` 与 `(2,3)` 均不能再选。最终 `E_o` 只能包含一条边。

### 13.3 OCS 命中测试

给定：

```text
E_o(t) contains (1,2)
L_12^ocs = 3
r_f = 2
Θ_o = 10
v_s = 1, v_d = 2
```

期望：

```text
path_type = OCS
path = (1,2)
L_12^ocs becomes 5
```

### 13.4 OCS 容量不足测试

给定：

```text
E_o(t) contains (1,2)
L_12^ocs = 9
r_f = 2
Θ_o = 10
```

期望：

```text
path_type = EPS
path = path_eps(1,2)
L_12^ocs remains 9
```

### 13.5 未命中 OCS 测试

给定：

```text
E_o(t) does not contain (1,3)
v_s = 1, v_d = 3
```

期望：

```text
path_type = EPS
path = path_eps(1,3)
```

### 13.6 已启动 flow 路径保持测试

给定：

```text
flow f 在周期 t 被分配到 EPS
周期 t+1 新建了对应 OCS 光路
```

期望：

```text
flow f 不迁移，继续使用原 EPS 路径直到完成
新到达的同源宿 flow 才重新判断是否可走 OCS
```

---

## 14. 一句话总结

第一版算法 V5 是一个“结构收益驱动的 OCS 直连光路调度 + 简单 OCS 命中优先路由”方案：控制器用上一窗口流量矩阵通过随机图零模型和 Louvain-style 社区划分计算候选光路收益，在端口约束下贪心建立 OCS 光路；数据面新 flow 若命中直连光路且容量可用则走 OCS，否则立即回退 EPS。
