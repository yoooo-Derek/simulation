# Legacy Reference Map

`/home/dyn/sim` is a read-only behavioral reference. No old main program or
large implementation block is migrated.

Reviewed reference areas:

- `/home/dyn/sim/src/main/hybrid-dcn-main.cc`
- `/home/dyn/sim/src/model/louvain.h`
- `/home/dyn/sim/src/ocs/ocs-state.h`
- `/home/dyn/sim/src/traffic/traffic-matrix.h`
- `/home/dyn/sim/src/metrics/trace-metrics.h`
- `/home/dyn/sim/src/result/structured-result-schema.h`
- `/home/dyn/sim/scripts/tl_ocs_experiments/`

Borrowed concepts:

- data-plane trace collection into a traffic matrix;
- degree-based random-background correction;
- community-aware optical edge scoring;
- optical-port-constrained greedy selection;
- active lightpath state and pair-based new-flow assignment;
- small smoke scenario shapes and CSV naming discipline.

Rewritten module boundaries:

- `observer` owns measured `W`;
- `algorithm` owns matrix processing, community detection, and edge selection;
- `routing` owns active lightpaths and path assignment;
- `applications` owns TCP application installation;
- `controller` owns one-cycle two-stage orchestration;
- `metrics` and `results` own trace-derived outputs.

Not migrated:

- the old monolithic main program;
- synthetic controller matrices;
- old route-binding implementations;
- structured result writer implementation;
- large experiment orchestration.
