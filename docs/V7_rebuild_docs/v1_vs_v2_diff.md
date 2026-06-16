# v1_vs_v2_diff.md

- V1 / V5：`TL-OCS`，重点是基于随机图零模型 + Louvain 社区划分的 OCS 光路调度；路由层较简单，即“命中直连 OCS 且容量可行则走 OCS，否则走 EPS”。
- V2 / V7：`TL-HOC`，是完整的“通信结构感知光电协同控制方法”；包含 `TL-OCS` 光拓扑动态调度 + `状态感知协同路由`。第二版把结构信息从调度阶段继续传递到流级路由阶段。

---

## 0. 一句话结论

V2 不是在 V1 上只改公式或参数，而是把仿真工程从“**光路调度 + 简单 OCS/EPS 二选一路由**”改成“**结构感知光拓扑生成 + 状态感知核心光路径选择 + 等待队列重试**”。因此原工程中只根据 `(src_tor, dst_tor)` 是否存在直连光路来决定路径的逻辑需要重写。

---

## 1. 总体控制链路变化

### V1 控制链路

V1 的主链路是：

```text
W^(t-1)
  -> A^(t-1)
  -> B^(t-1)
  -> Community C^(t-1)
  -> G_ij^(t-1)
  -> E_o(t)
  -> path(f)
```

其中 `path(f)` 的实现很简单：

```text
if direct_ocs_edge(src_access, dst_access) exists and capacity feasible:
    use one-hop OCS path
else:
    use EPS path
```

### V2 控制链路

V2 的主链路变成：

```text
W^(t-1)
  -> A^(t-1)
  -> S^(t-1)
  -> C^(t-1)
  -> G^(t-1)
  -> G_o(t)
  -> path(f)
```

关键变化：

1. 新增 `S_ij`：结构性通信强度。
2. 输出从 `E_o(t)` 扩展为 `G_o(t)=(V_o,E_o(t))`，即核心光拓扑。
3. `TL-OCS` 输出不再只给路由一个光路集合，还要输出：
   - 社区标签 `c_i`
   - 光路调度收益矩阵 `G_ij`
   - 光链路容量状态 `C_ij^o`
   - 光链路负载状态 `L_ij^o`
4. 路由阶段要使用这些结构状态，构造并比较：
   - 直接光路径 `P_dir`
   - 结构相关间接光路径 `P_rel`
   - 一般可达光路径 `P_reach`
5. 若没有可行核心光路径，跨组流不再直接走 EPS，而是进入等待队列 `Q(t)`。

---

## 2. 命名与概念差异

| 项目     | V1 / V5                 | V2 / V7                                             | 对工程的影响                                                                      |
| ------ | ----------------------- | --------------------------------------------------- | --------------------------------------------------------------------------- |
| 总方法名   | TL-OCS 光路调度与光电协同路由      | TL-HOC 通信结构感知光电协同控制                                 | 工程顶层策略类建议从 `TLOCSController` 扩展/重命名为 `TLHOCController`，内部保留 `TLOCScheduler` |
| 光路调度算法 | TL-OCS                  | TL-OCS 仍保留，但作为 TL-HOC 的第一阶段                         | 调度模块可复用主体，但输出接口要扩展                                                          |
| 路由算法   | 光路命中 + EPS 转发           | 状态感知协同路由                                            | 路由模块需要重写                                                                    |
| 结构指标   | `B_ij` 的正向偏离直接进入 `G_ij` | 显式定义 `S_ij=[B_ij]^+`，再由 `S_ij*h(c_i,c_j)` 得到 `G_ij` | 需要新增 `structural_strength_matrix S`，不要只在 `G` 中临时计算                          |
| 网络抽象   | EPS 静态网络 + OCS 动态直连光路   | 服务器集合、组内电网络、光接入节点、组间核心光拓扑                           | 拓扑对象从扁平 ToR/DCN 改为分层拓扑                                                      |
| 路由失败处理 | 走 EPS                   | 跨组流进入等待队列                                           | 需要新增 `WaitingQueue`、重试事件、流状态机                                               |
| 仿真目标   | 验证 TL-OCS 光路调度效果        | 验证 TL-HOC 中调度与路由协同效果                                | 实验对比组和指标需要调整                                                                |

---

## 3. 拓扑模型变化

### 3.1 V1 拓扑

V1 使用扁平混合 DCN：

```text
G_p(t) = (V, E_e ∪ E_o(t))
```

- `V`：接入节点 / ToR。
- `E_e`：EPS 静态链路，提供全网基础连通性。
- `E_o(t)`：OCS 动态光链路。
- 任意未命中 OCS 的 flow 可由 EPS 完成转发。

### 3.2 V2 拓扑

V2 使用分层混合网络：

```text
G(t) = (V_s, V_e, V_o, E_intra^e, E_o(t))
G_o(t) = (V_o, E_o(t))
```

- `V_s`：服务器节点集合。
- `V_e`：组内电交换节点集合。
- `V_o`：光接入节点集合。
- `E_intra^e`：组内电交换链路，只负责本地接入、本地转发、到光接入节点的转发。
- `E_o(t)`：组间核心光路集合。
- `G_o(t)`：当前周期核心光拓扑。

### 3.3 工程修改点

需要把原来的“EPS 提供所有接入节点之间基础路径”的假设拆开：

```text
旧：
  all nodes connected by EPS fallback
  OCS is optional acceleration path

新：
  intra-group traffic -> electrical intra-group path
  cross-group traffic -> source intra path + core optical path + destination intra path
  no feasible core optical path -> wait/retry, not EPS fallback
```

建议新增或修改数据结构：

```python
class HybridTopology:
    servers: List[ServerId]
    electrical_switches: List[SwitchId]
    optical_access_nodes: List[AccessNodeId]
    intra_electrical_edges: Graph
    optical_core_graph: DynamicGraph  # G_o(t)
    server_to_access: Dict[ServerId, AccessNodeId]  # g(h)
    server_to_group: Dict[ServerId, GroupId]
    access_to_group: Dict[AccessNodeId, GroupId]
```

---

## 4. 业务流与流量矩阵接口变化

### 4.1 V1

V1 的 flow 至少包含：

```text
f = (h_s, h_d, L_f) 或类似字段
```

路由时需要 `r_f` 做 OCS 光路容量判断，但 V1 中流模型主要强调 flow 大小、开始时间和源宿映射。

### 4.2 V2

V2 明确要求每条数据流包含速率估计：

```text
f = (h_s, h_d, L_f, r_f)
```

其中：

- `h_s`：源服务器。
- `h_d`：目的服务器。
- `L_f`：数据流大小。
- `r_f`：路由模块用于负载判断的速率估计。

有向流量矩阵由服务器级流集合聚合到光接入节点：

```text
w_ij^(t) = sum L_f
where f in F^(t), g(h_s)=v_i, g(h_d)=v_j, i != j
```

然后合并为无向通信强度：

```text
A_ij^(t) = w_ij^(t) + w_ji^(t)
A_ii^(t) = 0
```

### 4.3 工程修改点

1. `Flow` 类必须显式维护 `rate_estimate` / `r_f`。
2. 统计矩阵更新要基于 `g(h)`，从服务器映射到光接入节点，而不是只依赖 ToR 编号。
3. 组内流 `g(h_s)==g(h_d)`：
   - 不进入核心光拓扑路由。
   - 可不计入跨组核心矩阵，或保证 `w_ii=0`。
4. 跨组流 `g(h_s)!=g(h_d)`：
   - 计入 `W/A`。
   - 路由时需要核心光路径 `path_o(v_s,v_d)`。

---

## 5. 状态变量变化

### 5.1 V1 主要状态

| 状态            | 含义              |
| ------------- | --------------- |
| `W^(t)`       | 有向观测流量矩阵        |
| `A^(t)`       | 无向通信强度矩阵        |
| `d_i^(t)`     | 节点吞吐度           |
| `M^(t)`       | 有效总流量           |
| `P_ij^(t)`    | 随机图零模型期望通信强度    |
| `B_ij^(t)`    | 模块度增益 / 非均衡通信偏离 |
| `c_i`         | Louvain 社区标签    |
| `G_ij^(t)`    | 光调度增益           |
| `x_ij^(t)`    | 光路建立变量          |
| `E_o(t)`      | 当前 OCS 光链路集合    |
| `L_ij^ocs(t)` | 直连 OCS 光路当前负载   |
| `z_f^ocs(t)`  | flow 是否被分配到 OCS |

### 5.2 V2 新增 / 改名状态

| 状态              | 类型                        | 说明                     | 工程动作                 |
| --------------- | -------------------------:| ---------------------- | -------------------- |
| `V_s`           | set/list                  | 服务器集合                  | 新增                   |
| `V_e`           | set/list                  | 组内电交换节点集合              | 新增                   |
| `V_o`           | set/list                  | 光接入节点集合                | 替代 V1 的泛化 `V`        |
| `E_intra^e`     | graph                     | 组内电链路集合                | 替代全网 EPS fallback 模型 |
| `G_o(t)`        | graph                     | 当前周期核心光拓扑              | 新增，不能只保存 edge set    |
| `S_ij^(t)`      | matrix                    | 结构性通信强度，`max(B_ij,0)`  | 新增显式矩阵               |
| `L(C)`          | scalar per community      | 社区内部流量局部性增益            | 可用于统计/解释，非必要调度输入     |
| `R(C_p,C_q)`    | scalar per community-pair | 社区间通信关联度               | 可用于统计/解释，非必要调度输入     |
| `C_ij^o(t)`     | edge capacity             | 已建立光链路容量状态             | 新增                   |
| `L_ij^o(t)`     | edge load                 | 当前核心光链路负载估计            | 替代/规范化 `L_ij^ocs`    |
| `P_sd^dir`      | path set                  | 直接光路径候选集合              | 新增                   |
| `P_sd^rel`      | path set                  | 结构相关两跳路径候选集合           | 新增                   |
| `P_sd^reach`    | path set                  | 一般可达光路径候选集合            | 新增                   |
| `P_sd^feasible` | path set                  | 满足负载阈值的可行路径集合          | 新增                   |
| `Gamma(p)`      | scalar                    | 路径结构支撑度，路径上 `G_ij` 平均值 | 新增                   |
| `H(p)`          | int                       | 核心跳数                   | 新增                   |
| `L(p)`          | scalar                    | 路径最大当前链路负载             | 新增                   |
| `Q(t)`          | queue                     | 等待队列                   | 新增                   |
| `y_f,p^(t)`     | binary/logical            | flow 是否选择候选路径 p        | 可作为仿真记录或约束建模状态       |

### 5.3 删除或弱化的状态

| V1 状态               | V2 处理                                                       |
| ------------------- | ----------------------------------------------------------- |
| `z_f^ocs(t)`        | 不再是核心输出。V2 输出 `path(f)` 或 `WAIT_RETRY`。可由 path 类型派生是否使用光核心。 |
| `path_eps(v_s,v_d)` | 跨组 fallback 不再使用。仅保留组内电路径 `path_e(...)`。                    |
| 全局 `E_e`            | 被 `E_intra^e` 替代。                                           |

---

## 6. 目标函数变化

### 6.1 调度目标：基本延续但定义更严格

V1：

```text
maximize sum_{i<j} x_ij^(t) G_ij^(t)
subject to per-node port constraints and binary/undirected constraints
```

V2：

```text
maximize sum_{(v_i,v_j) in Ω^(t)} x_ij^(t) G_ij^(t-1)
```

V2 的变化：

1. 目标只在候选集合 `Ω^(t)` 上求和。
2. 候选集合新增跨组条件：

```text
Ω^(t) = {
  (v_i,v_j) |
  i<j,
  group(v_i) != group(v_j),
  G_ij^(t-1) > 0
}
```

3. `G_ij` 的构造从隐式 `[B_ij]^+ * h(...)` 改为显式：

```text
S_ij^(t) = [B_ij^(t)]^+
G_ij^(t) = S_ij^(t) * h(c_i,c_j)
```

4. 新增可选单周期光路总数约束：

```text
sum_{i<j} x_ij^(t) <= K_o
```

5. 新增候选边约束：

```text
x_ij^(t) = 0, if (v_i,v_j) not in Ω^(t)
```

### 6.2 路由目标：V1 无显式路径排序，V2 新增路径适配度准则

V1 路由目标近似为规则匹配：

```text
prefer direct OCS if exists and load feasible, otherwise EPS
```

V2 路由目标是词典序选择：

```text
PathSelect(p):
  load feasible filter
  -> path type priority
  -> min H(p)
  -> max Gamma(p)
  -> min L(p)
```

其中：

```text
H(p) = |p| - 1
Gamma(p) = (1 / H(p)) * sum_{(v_i,v_j) in p} G_ij^(t-1)
L(p) = max_{(v_i,v_j) in p} L_ij^o(t)
```

路径类型优先级：

```text
P_dir -> P_rel -> P_reach
```

---

## 7. 约束变化

### 7.1 调度约束

| 约束     | V1                  | V2                                   | 工程影响            |
| ------ | ------------------- | ------------------------------------ | --------------- |
| 端口约束   | `sum_j x_ij <= k`   | 保留                                   | 复用              |
| 二值约束   | `x_ij in {0,1}`     | 保留                                   | 复用              |
| 无向约束   | `x_ij=x_ji, x_ii=0` | 保留                                   | 复用              |
| 候选边约束  | 仅 `G_ij>0`          | `group(i)!=group(j)` 且 `G_ij>0`      | 候选边生成必须过滤同组节点   |
| 光路总数约束 | 无                   | 可选 `K_o`                             | 新增参数，若未配置可禁用    |
| 容量状态   | 路由中检查直连 OCS         | 调度输出后初始化每条光链路 `C_ij^o=C_o, L_ij^o=0` | 调度模块要初始化容量/负载状态 |

### 7.2 路由约束

V1：

```text
x_sd^(t) == 1
and L_sd^ocs(t) + r_f <= Theta_o
```

V2：

```text
for every edge (v_i,v_j) in candidate path p:
    L_ij^o(t) + r_f <= Theta_o
Theta_o <= C_o
```

变化点：

1. 负载约束从“直连边”扩展到“路径上每条光边”。
2. `Theta_o` 明确不能超过物理容量 `C_o`。
3. 候选路径可以是多跳光路径。
4. 若没有可行路径，进入等待队列，而不是 EPS。

---

## 8. TL-OCS 光拓扑调度算法变化

### 8.1 可复用部分

V1 中以下逻辑可以复用：

1. 根据 `A` 计算：
   - `d_i`
   - `M`
   - `P_ij = d_i*d_j/(2M)`
   - `B_ij = A_ij - eta * P_ij`
2. Louvain 社区划分。
3. 社区影响函数：

```text
h(c_i,c_j) = 1      if c_i == c_j
             alpha  otherwise
```

4. 候选边按 `G_ij` 降序排序。
5. 按端口约束贪心选边。

### 8.2 必须修改部分

V2 的 TL-OCS 调度步骤应改为：

```text
input:
  A^(t-1)
  V_o
  k
  C_o
  eta
  alpha
  K_o optional

output:
  G_o(t)=(V_o,E_o(t))
  x_ij^(t)
  c_i
  G_ij^(t-1)
  C_ij^o(t)
  L_ij^o(t)

algorithm:
  1. initialize E_o(t)=empty, x=0, deg_o=0
  2. compute d_i, M, P_ij, B_ij
  3. compute S_ij = max(B_ij, 0)
  4. run Louvain on weighted communication graph to get c_i
  5. compute G_ij = S_ij * h(c_i,c_j)
  6. build Ω(t) with:
       i < j
       group(v_i) != group(v_j)
       G_ij > 0
  7. sort Ω(t) by G_ij descending
  8. scan sorted edges:
       if deg_o[i] < k and deg_o[j] < k and K_o not exceeded:
           x_ij = 1
           add edge to E_o(t)
           deg_o[i] += 1
           deg_o[j] += 1
  9. build G_o(t)=(V_o,E_o(t))
 10. for each selected edge:
       C_ij^o(t)=C_o
       L_ij^o(t)=0
 11. return G_o, x, c, G, C_o_state, L_o_state
```

### 8.3 重要注意

- `S_ij` 要作为独立输出或至少作为可追踪中间状态保存，方便实验分析结构性通信强度。
- Louvain 仍应基于 `B_ij` / 模块度目标，而不是基于 `S_ij`。`S_ij` 用于边级结构强度和调度收益。
- 候选边要过滤同组节点，因为组内通信由电网络完成。
- 调度输出的 `G_ij` 后续要被路由阶段用于 `Gamma(p)`，不能只用于排序后丢弃。

---

## 9. 路由控制逻辑变化

### 9.1 V1 路由逻辑

```text
on flow arrival f:
  v_s = g(h_s)
  v_d = g(h_d)

  if (v_s,v_d) in E_o(t) and L_sd^ocs + r_f <= Theta_o:
      path = one-hop OCS(v_s,v_d)
      z_f^ocs = 1
      L_sd^ocs += r_f
  else:
      path = EPS(v_s,v_d)
      z_f^ocs = 0
```

### 9.2 V2 路由逻辑

```text
on flow arrival f=(h_s,h_d,L_f,r_f):
  v_s = g(h_s)
  v_d = g(h_d)

  if v_s == v_d:
      path(f) = path_e(h_s,h_d)
      return ASSIGNED

  # cross-group flow
  candidate_paths = []

  # 1. Direct optical path
  if (v_s,v_d) in E_o(t) and load_feasible((v_s,v_d), r_f):
      candidate_paths += [(v_s,v_d)]
      choose from P_dir and bind
      return ASSIGNED

  # 2. Structurally related two-hop path
  for relay v_r:
      if (v_s,v_r) in E_o(t) and (v_r,v_d) in E_o(t):
          if c_r == c_s or c_r == c_d:
              if all edges load-feasible:
                  candidate_paths += [(v_s,v_r,v_d)]
  if candidate_paths:
      choose by min H, max Gamma, min L
      bind path
      return ASSIGNED

  # 3. General reachable optical path
  P_reach = search_paths(G_o(t), v_s, v_d, H_max)
  P_reach = filter load-feasible paths
  if P_reach:
      choose by min H, max Gamma, min L
      bind path
      return ASSIGNED

  # 4. No feasible core optical path
  Q(t).push(f)
  return WAIT_RETRY
```

### 9.3 路径绑定和负载更新

V2 要求路径一旦确定，在 flow 传输期间保持绑定。

分配成功后：

```text
for edge in path_o(v_s,v_d):
    L_edge^o(t) += r_f
```

flow 完成后：

```text
for edge in path_o(v_s,v_d):
    L_edge^o(t) -= r_f
trigger waiting queue retry
```

### 9.4 等待队列重试触发条件

新增等待队列 `Q(t)`，用于跨组流无可行核心光路径时保存状态。

重试事件包括：

1. 已有 flow 完成并释放相关光链路负载。
2. 下一调度周期开始，`TL-OCS` 生成新的 `G_o(t)`。
3. 控制器周期性触发重试。

建议实现：

```python
class WaitingQueue:
    def enqueue(flow): ...
    def retry_on_flow_finish(released_edges): ...
    def retry_on_topology_update(new_G_o): ...
    def retry_periodic(now): ...
```

---

## 10. 通信机制变化

### 10.1 V1

- 调度模块向数据面下发直连 OCS 规则。
- 对每条光路 `(v_i,v_j)`，匹配对应源宿接入节点对。
- 未命中 OCS 规则的 flow 使用 EPS 默认规则。

### 10.2 V2

- 调度模块输出核心光拓扑 `G_o(t)` 和结构状态。
- 路由模块不是只查 `(src,dst)` 直连边，而是在 `G_o(t)` 上构造候选核心光路径。
- 数据面路径可能包括：
  - 组内电路径 `path_e(h_s,v_s)`
  - 一条或多条核心光链路组成的 `path_o(v_s,v_d)`
  - 目的组内电路径 `path_e(v_d,h_d)`
- 等待队列保存未获得核心光路径的跨组 flow。

### 10.3 工程影响

原先如果仿真中只有以下两类路径：

```text
OCS_DIRECT
EPS_FALLBACK
```

V2 至少需要扩展为：

```text
INTRA_ELECTRICAL
OPTICAL_DIRECT
OPTICAL_RELATED_2HOP
OPTICAL_REACHABLE_MULTIHOP
WAIT_RETRY
```

建议记录字段：

```python
flow.path_type
flow.core_path_hops
flow.core_path_edges
flow.path_structural_score  # Gamma(p)
flow.max_path_load          # L(p) at assignment
flow.wait_start_time
flow.wait_retry_count
```

---

## 11. 参数变化

### 11.1 V1 参数

| 参数        | 含义            |
| --------- | ------------- |
| `k`       | 单节点光端口上限      |
| `eta`     | 随机图零模型分辨率参数   |
| `alpha`   | 跨社区折减系数       |
| `Theta_o` | 单条 OCS 光路分配阈值 |
| `tau`     | 统计窗口          |
| `T_o`     | OCS 调度周期      |
| `C_e`     | EPS 链路容量      |
| `C_o`     | OCS 链路容量      |

### 11.2 V2 新增或语义改变参数

| 参数                            | 变化                                             |
| ----------------------------- | ---------------------------------------------- |
| `K_o`                         | 可选，单周期可建立核心光路总数上限                              |
| `H_max`                       | 一般可达光路径搜索最大核心跳数                                |
| `rho`                         | 业务注入负载比例，实验中用于多负载压力测试                          |
| `N=8` 主实验设置                   | V7 明确主实验为 128 台服务器、8 组、每组 16 台服务器、8 个光接入节点     |
| `E_intra^e` 相关参数              | 组内电网络容量、路径、交换结构需要显式建模                          |
| `Q_retry_interval`            | 论文未给符号，但工程需要周期性等待队列重试间隔                        |
| `flow.rate_estimation_method` | 论文使用 `r_f`，工程需要定义如何从 flow 大小、应用速率或 socket 估计速率 |

---

## 12. 输入/输出接口变化

### 12.1 TL-OCS 调度器接口

#### V1

```python
schedule(A, k, eta, alpha) -> E_o
```

或：

```python
schedule(A) -> selected_ocs_edges
```

#### V2

```python
schedule(
    A_prev,
    optical_access_nodes,
    group_of_access_node,
    k,
    C_o,
    eta,
    alpha,
    K_o=None,
) -> ScheduleResult
```

建议：

```python
@dataclass
class ScheduleResult:
    optical_core_graph: Graph              # G_o(t)
    selected_edges: Set[Edge]              # E_o(t)
    x: Dict[Edge, int]                     # x_ij^(t)
    community: Dict[AccessNodeId, int]     # c_i
    B: Matrix                              # optional diagnostics
    S: Matrix                              # S_ij
    gain: Matrix                           # G_ij
    capacity: Dict[Edge, float]            # C_ij^o(t)
    load: Dict[Edge, float]                # L_ij^o(t), initialized to 0
```

### 12.2 路由器接口

#### V1

```python
route_flow(flow, E_o, optical_load, Theta_o) -> PathResult
```

#### V2

```python
route_flow(
    flow,
    topology,
    schedule_result,
    optical_load,
    Theta_o,
    H_max,
    waiting_queue,
) -> RouteResult
```

建议：

```python
@dataclass
class RouteResult:
    status: Literal["ASSIGNED", "WAIT_RETRY"]
    path_type: Literal[
        "INTRA_ELECTRICAL",
        "OPTICAL_DIRECT",
        "OPTICAL_RELATED_2HOP",
        "OPTICAL_REACHABLE_MULTIHOP",
        "WAIT_RETRY",
    ]
    end_to_end_path: Optional[List[NodeId]]
    core_path: Optional[List[AccessNodeId]]
    core_edges: List[Edge]
    H: Optional[int]
    Gamma: Optional[float]
    max_load: Optional[float]
```

### 12.3 控制器主循环接口

V2 主循环需要支持初始化阶段与周期更新：

```python
# initialization
A0 = warmup_observation() or build_from_job_profile()
schedule_result = tl_ocs.schedule(A0, ...)

# during simulation
on_flow_arrival(flow):
    result = router.route_flow(flow, topology, schedule_result, ...)
    if result.status == "WAIT_RETRY":
        waiting_queue.enqueue(flow)

on_flow_finish(flow):
    release_optical_load(flow.core_edges, flow.r_f)
    waiting_queue.retry_related(...)

on_schedule_period_boundary(t):
    A = aggregate_observed_matrix(t)
    schedule_result = tl_ocs.schedule(A, ...)
    waiting_queue.retry_all(...)
```

---

## 13. 仿真实验设计变化

### 13.1 拓扑与环境

V1：

- 泛化接入节点数量 `N`。
- 每个接入节点连接若干服务器 `S`。
- EPS 全网基础连通 + OCS 动态直连。

V2：

- 主实验明确采用：
  - 128 台服务器。
  - 8 个服务器组。
  - 每组 16 台服务器。
  - 每组对应 1 个光接入节点。
  - 因此 `N=8`。
- 网络为组内电交换 + 组间光交换核心。

### 13.2 流量模型

V1 三类流量：

1. 均匀背景流量。
2. 社区内高通信流量。
3. 参数聚合流量。

V2 三类流量语义保留但表述和实验目的更新：

1. 均匀背景流量：弱结构通信背景。
2. 社区型训练流量：数据并行组、张量并行组、局部同步组等。
3. 聚合型训练流量：参数聚合、梯度汇聚、专家并行中的中心化通信。

V2 仍使用混合流大小分布：

```text
L_f = L_s with probability p_s
L_f = L_l with probability 1-p_s
```

但 flow 定义必须包括 `r_f`。

### 13.3 对比方案变化

V1 对比方案：

| 策略         | 描述                 |
| ---------- | ------------------ |
| EPS        | 关闭 OCS，全部 EPS      |
| OCS-Volume | 按 `A_ij` 绝对流量排序建光路 |
| TL-OCS     | 结构收益调度 + 简单路由      |

V2 对比方案：

| 策略            | 光路调度依据       | 路由依据        | 工程实现要求         |
| ------------- | ------------ | ----------- | -------------- |
| 纯电网络          | 无核心光路        | 电网络转发       | baseline       |
| 静态光拓扑         | 固定拓扑         | 当前拓扑可达路径    | 新增 baseline    |
| 流量排序光路调度      | `A_ij^(t-1)` | 最短路径或负载感知路径 | 扩展原 OCS-Volume |
| TL-OCS + 最短路径 | `G_ij^(t-1)` | 普通最短路径      | 新增消融实验         |
| TL-HOC        | `G_ij^(t-1)` | 路径适配度准则     | 完整方法           |

### 13.4 评价指标变化

V1 指标：

1. 平均 flow 完成时间。
2. 90% / 95% 尾部 flow 完成时间。
3. 平均接收吞吐。
4. OCS flow 命中率与字节命中率。
5. OCS 光链路利用率。
6. EPS 链路平均利用率与最大利用率。
7. 社区内光路比例。

V2 主指标收敛为：

1. 平均吞吐量：

```text
T_avg = sum Bytes_f^recv / T_sim
```

2. 平均接收端吞吐：

```text
T_recv_avg = (1/|R|) * sum_r Bytes_r^recv / T_sim
```

3. 平均链路利用率：

```text
U_avg = (1/|E|) * sum_e Bytes_e / (C(e)*T_sim)
```

建议工程上**同时保留 V1 的 FCT、尾延迟、命中率等指标作为补充诊断**，但论文 V7 的主图/主表应以三类 V2 指标为核心。

### 13.5 实验流程变化

V2 增加明确流程：

1. 初始化服务器、组内电网络、光接入节点和光交换核心。
2. 生成完整数据流序列。
3. 所有策略使用相同流序列。
4. 预热窗口收集 `A^(0)`。
5. 启用光核心的策略生成初始 `G_o(0)`。
6. 正式阶段周期性收集 `W^(t)` / `A^(t)` 并更新光拓扑。
7. flow 到达时按当前策略选择端到端路径并更新链路负载。
8. 记录接收字节、链路发送字节、路径类型、核心路径跳数、调度时间、路由计算时间。
9. 多随机种子统计平均。

---

## 14. 代码模块级修改建议

### 14.1 Scheduler 模块

保留：

- `compute_degree(A)`
- `compute_total_traffic(A)`
- `compute_null_model(d,M)`
- `compute_B(A,P,eta)`
- `run_louvain(B or weighted graph)`
- `greedy_select_edges_by_gain(...)`

新增/修改：

```python
compute_structural_strength(B) -> S
compute_gain(S, communities, alpha) -> G
build_candidate_edges(G, group_of_access, require_cross_group=True) -> Omega
greedy_select_edges(Omega, G, k, K_o=None) -> E_o, x
initialize_optical_edge_state(E_o, C_o) -> capacity, load
return ScheduleResult(...)
```

### 14.2 Router 模块

删除/禁用：

```python
if no direct OCS: use EPS fallback for cross-group flow
```

新增：

```python
is_intra_group(flow)
get_direct_path(v_s, v_d)
get_related_two_hop_paths(v_s, v_d, communities)
search_reachable_paths(G_o, v_s, v_d, H_max)
filter_load_feasible(paths, optical_load, r_f, Theta_o)
compute_H(path)
compute_Gamma(path, gain_matrix)
compute_L(path, optical_load)
select_path_lexicographic(paths)
bind_flow_to_path(flow, path)
release_flow_path(flow)
```

### 14.3 Controller 模块

新增：

```python
warmup_observation_window
schedule_period_boundary_handler
waiting_queue_retry_handler
flow_finish_handler
```

### 14.4 Metrics 模块

新增或确保输出：

```python
avg_throughput
avg_receiver_throughput
avg_link_utilization
path_type_counts
core_path_hop_distribution
routing_compute_time
scheduling_compute_time
waiting_queue_delay
waiting_retry_count
```

保留作为补充：

```python
flow_completion_time_mean
flow_completion_time_p90
flow_completion_time_p95
optical_flow_hit_rate
optical_byte_hit_rate
community_internal_edge_ratio
```

---

## 15. 关键行为测试用例

### 15.1 结构性通信强度测试

输入一个小矩阵 `A`，手工计算：

```text
d_i, M, P_ij, B_ij, S_ij
```

断言：

```text
S_ij == max(B_ij, 0)
G_ij == S_ij if same community
G_ij == alpha*S_ij if different community
```

### 15.2 候选边过滤测试

构造两个同组 access nodes，使 `G_ij>0`。

V2 断言：

```text
(v_i,v_j) not in Ω(t) if group(v_i)==group(v_j)
```

### 15.3 `K_o` 测试

设置 `K_o=1`，即使端口还有空余，也最多选择一条光路。

### 15.4 直接光路径测试

若 `(v_s,v_d) in E_o(t)` 且容量可行：

```text
path_type == OPTICAL_DIRECT
core_path == [v_s,v_d]
```

### 15.5 结构相关两跳路径测试

若无直连，但存在：

```text
(v_s,v_r), (v_r,v_d) in E_o(t)
c_r == c_s or c_r == c_d
```

且容量可行，则：

```text
path_type == OPTICAL_RELATED_2HOP
core_path == [v_s,v_r,v_d]
```

### 15.6 一般可达路径测试

若无直连、无结构相关两跳，但 `G_o(t)` 中存在不超过 `H_max` 的可达路径，则：

```text
path_type == OPTICAL_REACHABLE_MULTIHOP
```

### 15.7 等待队列测试

若跨组流无任何可行核心光路径：

```text
status == WAIT_RETRY
flow in Q(t)
optical load unchanged
```

当相关光链路释放或拓扑更新后，重试成功：

```text
flow removed from Q(t)
path assigned
optical load updated
```

### 15.8 路径排序测试

同一类型、同跳数候选路径：

1. 选择 `Gamma(p)` 最大者。
2. 若 `Gamma` 相同，选择 `L(p)` 最小者。

---

## 16. 最小可行改造路线

建议 Codex 按以下顺序修改，降低破坏已有工程的风险：

1. **先改数据结构**：加入 `V_s/V_e/V_o/E_intra^e/G_o/server_to_access/group`。
2. **改流对象**：确保 `Flow` 包含 `r_f`。
3. **改 TL-OCS 输出**：返回 `ScheduleResult`，包含 `S/G/community/capacity/load/G_o`。
4. **实现 V2 Router**：先实现直接光路径，再实现两跳结构相关路径，最后实现一般可达路径和 `H_max`。
5. **加入等待队列**：先支持无路径入队和拓扑更新后全量重试，再优化成相关边释放触发。
6. **改实验策略**：新增静态光拓扑、TL-OCS+最短路径、TL-HOC。
7. **改指标输出**：优先实现 V7 三个主指标，再保留 V5 诊断指标。
8. **跑单元测试**：覆盖上面 15 节行为。
9. **跑小规模集成实验**：例如 8 个 access nodes、每组少量服务器、三类流量各跑一次。

---

## 17. Codex 修改时的禁止事项

1. 不要把 V2 的跨组无路径情况 fallback 到 EPS。V2 的核心区别之一就是等待队列。
2. 不要在调度完成后丢弃 `G_ij` 和 `c_i`，路由阶段必须使用。
3. 不要用 `S_ij` 替代 Louvain 的 `B_ij` 模块度目标；`S_ij` 是正向结构强度，`B_ij` 仍保留正负偏离用于社区划分。
4. 不要只支持一跳 OCS；V2 必须支持两跳结构相关路径和一般可达多跳路径。
5. 不要把组内电网络理解成全局 EPS fallback；`E_intra^e` 是组内接入和本地转发。
6. 不要把 `Theta_o` 当成链路物理容量本身；它是可分配负载阈值，并应满足 `Theta_o <= C_o`。
7. 不要只比较 EPS、OCS-Volume、TL-OCS 三组；V2 需要 TL-HOC 消融对比。

---

## 18. 最终验收标准

仿真工程修改完成后，应满足：

1. 能从同一 flow 序列生成 `W^(t)` 和 `A^(t)`。
2. `TL-OCS` 能输出 `G_o(t)`、`E_o(t)`、`c_i`、`S_ij`、`G_ij`、`C_ij^o`、`L_ij^o`。
3. 跨组 flow 能按以下顺序路由：

```text
direct optical -> related two-hop optical -> reachable optical -> wait retry
```

4. 等待队列能在 flow 完成、拓扑更新或周期性事件后重试。
5. 输出至少包含：

```text
T_avg
T_recv_avg
U_avg
path_type_counts
avg_core_hops
waiting_queue_delay or wait_count
scheduling_time
routing_time
```

6. 对比策略至少包含：

```text
Pure electrical
Static optical topology
Volume-based optical scheduling
TL-OCS + shortest path
TL-HOC
```

7. 能在均匀背景、社区型训练、聚合型训练三类流量场景下重复运行多随机种子实验。
