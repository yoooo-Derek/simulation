# TL-OCS Architecture

The new project is an ns-3.47 source tree. TL-OCS self-authored code belongs in
`contrib/tl-ocs`. Scratch programs are thin entry points only.

## Module Boundary

`contrib/tl-ocs` is the project module. It maps V3 into the following boundaries:

- `config`: simulation and controller parameters, validation, and preset loading.
- `traffic`: future ToR-pair traffic matrix inputs, synthetic training traffic
  descriptions, and trace adapters.
- `observer`: future data-plane counters that build `W(t)` from ns-3 packets or
  flow events.
- `algorithm`: future null-model, modularity gain, community detection, OCS gain,
  and constrained link-selection logic.
- `controller`: future periodic orchestration that connects observer state,
  algorithm output, OCS admission, EPS fallback, and rule updates.
- `topology`: future EPS and OCS physical/logical topology construction.
- `routing`: future OCS hit handling, EPS fallback, ECMP, and WECMP path binding.
- `applications`: future training-flow generators and ns-3 application helpers.
- `metrics`: future flow completion time, throughput, utilization, OCS hit rate,
  and reconfiguration counters.
- `results`: future structured CSV or artifact export paths and schemas.

Only `config` exists in this round. The other boundaries are documented now to
prevent new logic from accumulating in scratch.

## V3 Pipeline Mapping

V3's control flow maps to module responsibilities as follows:

- `W(t)` directed observation: `observer`.
- `A(t)` undirected matrix and EWMA `Abar(t)`: `traffic` plus `observer`.
- sparse traffic graph `G_f(t)`: `traffic`.
- null-model expectation and modularity gain `B_ij`: `algorithm`.
- Louvain-style traffic community detection: `algorithm`.
- OCS candidate gain and port-constrained selection: `algorithm`.
- state holding, update gate, and hold-time constraints: `controller` plus
  `algorithm`.
- OCS admission and EPS residual routing: `routing` plus `controller`.
- WECMP residual splitting: `routing`.
- topology and link capacity realization: `topology`.
- experiment traffic and evaluation metrics: `applications`, `metrics`, and
  `results`.

## Scratch Boundary

`scratch/tl-ocs-runner.cc` is a smoke runner. It may parse command-line options,
construct `SimulationConfig`, call `Validate()`, print a concise summary, and
run an empty ns-3 simulator interval. It must not contain TL-OCS algorithms,
topology construction, traffic generation, routing, or result export logic.

