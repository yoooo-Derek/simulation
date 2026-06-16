# Simulation Project Overview



## 1. 工程定位

当前工程是在 NS-3 主树上增加了一个自定义 contrib 模块：

- `contrib/tl-ocs/`：旧版 TL-OCS 仿真模块，包含算法、拓扑、流量生成、控制器、路由、指标和结果写出。
- `scratch/tl-ocs-runner.cc`：主要仿真入口，绝大多数实验脚本最终都运行这个 runner。
- `experiments/configs/`：轻量实验配置文件，采用 `key=value` 形式。
- `experiments/scripts/`：实验运行、结果校验和结果聚合脚本。
- `docs/paper/V5.md`：旧版算法论文说明。用户口头提到的旧 V1 工程，对应当前代码中的 V5/TL-OCS 旧算法实现。

## 2. 构建和运行

在仓库根目录运行：

```bash
./ns3 build
```

运行主入口的最小形式：

```bash
./ns3 run "tl-ocs-runner --enableEpsTopology=true --enableSchemeRunner=true --schemeName=tl-ocs"
```

更常用的是通过 `experiments/scripts/` 下的脚本运行。脚本会读取 `experiments/configs/*.properties`，把每一行 `key=value` 转成命令行参数 `--key=value`，然后调用：

```bash
./ns3 run "tl-ocs-runner ${args[*]}"
```

注意：`scratch/tl-ocs-runner.cc` 当前没有 `--configFile` 参数，配置文件不是由 C++ 代码直接读取的，而是由 shell 脚本转换成命令行参数。

## 3. 主入口

主入口文件：

- `scratch/tl-ocs-runner.cc`

核心职责：

- 解析所有命令行参数，例如 `numTors`、`serversPerTor`、`schemeName`、`trafficPattern`、`outputDir`、`summaryFile`、`flowResultFile`。
- 构造 `SimulationConfig`、`ExperimentConfig`、`OutputConfig`。
- 构建 EPS 拓扑和可选 OCS candidate links。
- 根据 `schemeName` 选择实验方案。
- 调用 traffic generator 生成流。
- 调用 `SmokeScenarioRunner` 或更底层的 controller/algorithm 路径执行仿真。
- 最后调用 `ResultWriter` 和 `FlowResultWriter` 写 CSV 结果。

关键参数：

- `--schemeName=eps-ecmp|ocs-volume|tl-ocs|ocs-oracle|fixed-ocs`
- `--trafficPattern=community-local|parameter-aggregation|...`
- `--enableSchemeRunner=true`：走统一 scheme runner。
- `--enableFlowMetrics=true`：生成 flow-level CSV。
- `--enableLinkMetrics=true`：生成链路利用率指标。
- `--enableOcsMetrics=true`：生成 OCS 命中率和重配置次数指标。
- `--outputDir=results/raw`：输出目录。
- `--summaryFile=xxx.csv`：summary CSV 文件名。
- `--flowResultFile=xxx-flows.csv`：flow-level CSV 文件名。

## 4. 旧版算法代码位置

旧版 TL-OCS/V1 算法核心位于：

- `contrib/tl-ocs/model/algorithm/tl-ocs-algorithm.{h,cc}`：算法总入口 `TlOcsAlgorithm::Run()`。
- `contrib/tl-ocs/model/algorithm/matrix-processor.{h,cc}`：从观测矩阵构造无向矩阵、稀疏化图。
- `contrib/tl-ocs/model/algorithm/null-model.{h,cc}`：计算 modularity gain 矩阵。
- `contrib/tl-ocs/model/algorithm/community-detector.{h,cc}`：社区检测。
- `contrib/tl-ocs/model/algorithm/optical-scheduler.{h,cc}`：根据算法得分选择 OCS edges。
- `contrib/tl-ocs/model/algorithm/baseline-schedulers.{h,cc}`：baseline 调度器。
- `contrib/tl-ocs/model/algorithm/traffic-graph.{h,cc}`：算法使用的 traffic graph 表示。

旧算法主流程在 `TlOcsAlgorithm::Run()` 中：

1. 从观测矩阵 `W` 构造无向矩阵 `A`。
2. 使用 `thetaF` 删除弱关系。
3. 构造 traffic graph。
4. 根据 `eta` 计算 null-model/modularity gain 矩阵 `B`。
5. 做 community detection。
6. 使用 `alpha` 和 `opticalPortsPerTor` 做 optical scheduling。
7. 返回 candidate edges、selected edges 和社区内部边比例。

## 5. 控制器、拓扑、路由和流量模块

重要模块路径：

- `contrib/tl-ocs/model/topology/eps-topology-builder.{h,cc}`：EPS 拓扑构建。
- `contrib/tl-ocs/model/topology/node-index.{h,cc}`：节点、服务器、ToR、OCS link 的索引和地址查询。
- `contrib/tl-ocs/model/observer/traffic-observer.{h,cc}`：观测流量并生成 `W(t)`。
- `contrib/tl-ocs/model/observer/traffic-matrix.{h,cc}`：流量矩阵表示。
- `contrib/tl-ocs/model/controller/controller-timeline.{h,cc}`：控制器时间线、两阶段 smoke、多周期调度。
- `contrib/tl-ocs/model/controller/controller-state.{h,cc}`：控制器状态。
- `contrib/tl-ocs/model/routing/flow-path-selector.{h,cc}`：流路径选择。
- `contrib/tl-ocs/model/routing/ocs-admission.{h,cc}`：OCS admission。
- `contrib/tl-ocs/model/routing/ocs-link-manager.{h,cc}`：OCS lightpath 激活/查询。
- `contrib/tl-ocs/model/applications/flow-launcher.{h,cc}`：安装并启动流。

流量生成器：

- `contrib/tl-ocs/model/traffic/community-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/aggregation-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/aggregation-distractor-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/matrix-replay-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/mechanism-separation-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/datapath-diagnostic-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/training-traffic-generator.{h,cc}`
- `contrib/tl-ocs/model/traffic/uniform-traffic-generator.{h,cc}`

## 6. Scheme runner

统一方案入口：

- `contrib/tl-ocs/model/experiments/scheme-config.{h,cc}`
- `contrib/tl-ocs/model/experiments/smoke-scenario-runner.{h,cc}`

`SchemeConfig::FromString()` 支持：

- `eps-ecmp`
- `ocs-volume`
- `tl-ocs`
- `ocs-oracle`
- `fixed-ocs`

`SmokeScenarioRunner::Run()` 根据 scheme 决定是否启用 OCS links、traffic observer、旧算法、OCS admission。`eps-ecmp` 不跑算法；`ocs-volume`、`tl-ocs`、`ocs-oracle`、`fixed-ocs` 会走 OCS 相关路径。

## 7. 配置文件

配置文件目录：

- `experiments/configs/`

当前保留的配置类型：

- `smoke-*.properties`：轻量 scheme smoke。
- `metrics-smoke-*.properties`：打开 flow metrics 的 smoke。
- `util-smoke-*.properties`：打开 link/OCS utilization metrics 的 smoke。
- `sanity-*.properties`：8/16 ToR sanity 配置。
- `scale8-observer.properties`：observer 相关小规模配置。

配置文件格式是每行一个 `key=value`，例如：

```properties
numTors=8
serversPerTor=2
spines=2
experimentName=phase11a-tl-ocs
schemeName=tl-ocs
trafficPattern=community-local
outputDir=results/raw
summaryFile=phase11a-tl-ocs.csv
flowResultFile=phase11a-tl-ocs-flows.csv
enableSchemeRunner=true
enableFlowMetrics=true
thetaF=0
eta=1.0
alpha=0.5
opticalPortsPerTor=1
```

这些 key 必须对应 `scratch/tl-ocs-runner.cc` 中 `cmd.AddValue()` 注册的参数。

## 8. 实验脚本

脚本目录：

- `experiments/scripts/`

运行脚本：

- `run-scheme-smoke.sh <eps-ecmp|ocs-volume|tl-ocs>`：读取 `smoke-*.properties`。
- `run-metrics-smoke.sh <eps-ecmp|ocs-volume|tl-ocs>`：读取 `metrics-smoke-*.properties`。
- `run-util-smoke.sh <eps-ecmp|ocs-volume|tl-ocs>`：读取 `util-smoke-*.properties`。
- `run-scale-sanity.sh <8|16> <eps-ecmp|ocs-volume|tl-ocs>`：读取 `sanity-<scale>tor-<scheme>.properties`。
- `run-all-scheme-smokes.sh`：依次跑三个 scheme smoke。
- `run-all-metrics-smokes.sh`：依次跑三个 metrics smoke。
- `run-all-util-smokes.sh`：依次跑三个 utilization smoke。
- `run-all-sanity.sh`：跑 sanity，聚合 summary，并校验 summary/flows。

结果处理脚本：

- `aggregate-results.py`：把多个 summary CSV 合并为一个表，默认输出 `results/tables/summary-table.csv`。
- `validate-results.py`：校验 summary CSV 和 flow-level CSV schema/数值范围。
- `diagnose-flows.py`：流级诊断工具，后续需要结合 V7 输出格式再判断是否继续使用。

常用命令：

```bash
./experiments/scripts/run-scheme-smoke.sh tl-ocs
./experiments/scripts/run-metrics-smoke.sh tl-ocs
./experiments/scripts/run-util-smoke.sh tl-ocs
./experiments/scripts/run-scale-sanity.sh 16 tl-ocs
./experiments/scripts/run-all-sanity.sh
```

## 9. 结果文件如何生成

默认输出目录：

- `results/raw/`

summary CSV：

- 由 `contrib/tl-ocs/model/results/result-writer.{h,cc}` 中的 `ResultWriter::WriteSmokeSummary()` 写出。
- 输出路径为 `outputDir/summaryFile`。
- `outputDir` 和 `summaryFile` 来自命令行或 `.properties`。

flow-level CSV：

- 由 `contrib/tl-ocs/model/results/flow-result-writer.{h,cc}` 中的 `FlowResultWriter::Write()` 写出。
- 只有打开 `enableFlowMetrics=true` 时生成。
- 输出路径为 `outputDir/flowResultFile`。
- 如果未显式设置 `flowResultFile`，runner 默认使用 `<experimentName>-flows.csv`。

summary CSV 字段定义：

- `contrib/tl-ocs/model/results/csv-schema.cc`
- 函数：`GetSmokeSummaryCsvHeader()`

flow-level CSV 字段定义：

- `contrib/tl-ocs/model/results/csv-schema.cc`
- 函数：`GetFlowResultCsvHeader()`

聚合表：

- `experiments/scripts/aggregate-results.py` 读取 summary CSV。
- 默认输出 `results/tables/summary-table.csv`。
- `run-all-sanity.sh` 会输出 `results/tables/sanity-summary.csv`。

## 10. 测试

测试文件目录：

- `contrib/tl-ocs/test/`

每个测试文件对应一个 ns-3 test suite，例如：

- `tl-ocs-algorithm-test-suite.cc`
- `tl-ocs-community-detector-test-suite.cc`
- `tl-ocs-optical-scheduler-test-suite.cc`
- `tl-ocs-smoke-scenario-runner-test-suite.cc`
- `tl-ocs-flow-path-selector-test-suite.cc`
- `tl-ocs-result-writer-test-suite.cc`

运行单个 suite：

```bash
./test.py -s tl-ocs-algorithm
./test.py -s tl-ocs-smoke-scenario-runner
```

修改 V7 时，建议优先保留并更新这些测试，而不是删除。

## 11. 后续 V7 改造时优先关注

如果 V7 只改算法逻辑，优先看：

- `contrib/tl-ocs/model/algorithm/tl-ocs-algorithm.{h,cc}`
- `contrib/tl-ocs/model/algorithm/matrix-processor.{h,cc}`
- `contrib/tl-ocs/model/algorithm/null-model.{h,cc}`
- `contrib/tl-ocs/model/algorithm/community-detector.{h,cc}`
- `contrib/tl-ocs/model/algorithm/optical-scheduler.{h,cc}`
- `contrib/tl-ocs/test/tl-ocs-algorithm-test-suite.cc`
- `contrib/tl-ocs/test/tl-ocs-optical-scheduler-test-suite.cc`

如果 V7 改实验流程或仿真控制：

- `scratch/tl-ocs-runner.cc`
- `contrib/tl-ocs/model/experiments/smoke-scenario-runner.{h,cc}`
- `contrib/tl-ocs/model/controller/controller-timeline.{h,cc}`
- `experiments/configs/*.properties`
- `experiments/scripts/*.sh`

如果 V7 改指标或结果格式：

- `contrib/tl-ocs/model/metrics/`
- `contrib/tl-ocs/model/results/csv-schema.cc`
- `contrib/tl-ocs/model/results/result-writer.cc`
- `contrib/tl-ocs/model/results/flow-result-writer.cc`
- `experiments/scripts/aggregate-results.py`
- `experiments/scripts/validate-results.py`

## 12. 当前工程的一个典型数据流

```text
experiments/configs/*.properties
    -> experiments/scripts/*.sh
    -> ./ns3 run "tl-ocs-runner --key=value ..."
    -> scratch/tl-ocs-runner.cc
    -> SimulationConfig / ExperimentConfig / OutputConfig
    -> EPS topology + traffic generator
    -> TrafficObserver builds W(t)
    -> TlOcsAlgorithm::Run()
    -> ControllerTimeline / OcsLinkManager / FlowPathSelector
    -> MetricsCollector / LinkMetricsCollector / OcsMetrics
    -> ResultWriter / FlowResultWriter
    -> results/raw/*.csv
    -> aggregate-results.py / validate-results.py
    -> results/tables/*.csv
```
