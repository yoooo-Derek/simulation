# Phase 15D Aggregation Report

This report aggregates Phase 15C raw CSV files for data-quality and statistics preparation. It is not paper text and does not state paper conclusions.

## Inputs

- Source raw files: `results/raw/phase15c-*.csv`
- Summary CSV count: 72
- Per-flow CSV count: 72
- Experiment commit for raw data: `c2e0c22 Add V5 alignment and calibration reports`
- Current documentation commit before Phase 15D: `3bf86be Record V5 phase15c formal raw batch`

## Outputs

- `results/processed/phase15c-summary-aggregate.csv`
- `results/processed/phase15c-scheme-comparison.csv`
- `results/processed/phase15c-quality-report.csv`

The processed CSV files are generated artifacts and are not intended to be committed by default.

## Quality Review

- Expected runs: `72`
- Observed summary files: `72`
- Observed per-flow files: `72`
- Missing summary files: `0`
- Missing per-flow files: `0`
- Failed runs: `0`
- Runs with incomplete flows: `0`
- Same-seed flow sequence alignment: `passed`
- EPS-only zero OCS checks: `passed`
- Path type domain checks: `passed`
- FCT nonnegative checks: `passed`
- Hit-rate range checks: `passed`
- Utilization range checks: `passed`

No failed, missing, or incomplete-flow runs were found in the Phase 15C raw set.

## Group Scheme Aggregate Summary

### uniform-main

- `eps-ecmp`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000758892726563` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00018356848` / `0.00125951061333`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000755111085938` / `0.002136126` / `0.002136126`, OCS assigned `5`, EPS fallback `251`, OCS hit `0.01953125`, OCS byte hit `0.0218802724211`, EPS avg/max util `0.00017981566` / `0.00125003093333`, OCS avg/max util `1.24268758908e-05` / `0.00107732533333`, non-empty rounds `29.3333333333`, avg active edges `1.9387755102`, active lightpath seconds `0.95`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000755111085938` / `0.002136126` / `0.002136126`, OCS assigned `5`, EPS fallback `251`, OCS hit `0.01953125`, OCS byte hit `0.0218802724211`, EPS avg/max util `0.00017981566` / `0.00125003093333`, OCS avg/max util `1.22943916365e-05` / `0.00107732533333`, non-empty rounds `29.3333333333`, avg active edges `1.96598639456`, active lightpath seconds `0.963333333333`, community ratio `0`.

### community-main

- `eps-ecmp`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000758852692708` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00018356848` / `0.00134517546667`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000716285134115` / `0.002136126` / `0.002136126`, OCS assigned `47`, EPS fallback `209`, OCS hit `0.18359375`, OCS byte hit `0.176439872006`, EPS avg/max util `0.00015265114` / `0.00117167061333`, OCS avg/max util `8.67903748083e-05` / `0.00103287146667`, non-empty rounds `29.3333333333`, avg active edges `2.01360544218`, active lightpath seconds `0.986666666667`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000713618459636` / `0.002136126` / `0.002136126`, OCS assigned `48.3333333333`, EPS fallback `207.666666667`, OCS hit `0.188802083333`, OCS byte hit `0.189726204838`, EPS avg/max util `0.00015037346` / `0.00117167061333`, OCS avg/max util `8.98880571533e-05` / `0.000989163885714`, non-empty rounds `29.3333333333`, avg active edges `2.04761904762`, active lightpath seconds `1.00333333333`, community ratio `0`.

### aggregation-main

- `eps-ecmp`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000856091767578` / `0.00215556433333` / `0.00217266366667`, OCS assigned `0`, EPS fallback `512`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00036437408` / `0.01165997056`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000845907266276` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS hit `0.03515625`, OCS byte hit `0.0363917084606`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857143` / `0.00125178628571`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000845907266276` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS hit `0.03515625`, OCS byte hit `0.0363917084606`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857143` / `0.00125178628571`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.

### aggregation-thetaf

- `eps-ecmp`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000856091767578` / `0.00215556433333` / `0.00217266366667`, OCS assigned `0`, EPS fallback `512`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00036437408` / `0.01165997056`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000845907266276` / `0.002155136` / `0.002176156`, OCS assigned `18`, EPS fallback `494`, OCS hit `0.03515625`, OCS byte hit `0.0363917084606`, EPS avg/max util `0.0003517886` / `0.0114103104`, OCS avg/max util `0.000400173857143` / `0.00125178628571`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000846675832031` / `0.002155136` / `0.002176156`, OCS assigned `17.3333333333`, EPS fallback `494.666666667`, OCS hit `0.0338541666667`, OCS byte hit `0.0322509217111`, EPS avg/max util `0.00035325768` / `0.01145732096`, OCS avg/max util `0.000341621619048` / `0.00125178628571`, non-empty rounds `11.6666666667`, avg active edges `0.238095238095`, active lightpath seconds `0.116666666667`, community ratio `0`.

### community-k2

- `eps-ecmp`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000758852692708` / `0.002136126` / `0.002136126`, OCS assigned `0`, EPS fallback `256`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00018356848` / `0.00134517546667`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000705949753907` / `0.002136126` / `0.002136126`, OCS assigned `58.3333333333`, EPS fallback `197.666666667`, OCS hit `0.227864583333`, OCS byte hit `0.217875346082`, EPS avg/max util `0.00014550974` / `0.00112466005333`, OCS avg/max util `7.30881575161e-05` / `0.000978884`, non-empty rounds `29.3333333333`, avg active edges `2.68707482993`, active lightpath seconds `1.31666666667`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `256/256`, throughput `253760000`, avg/p90/p95 FCT `0.000705949753907` / `0.002136126` / `0.002136126`, OCS assigned `58.3333333333`, EPS fallback `197.666666667`, OCS hit `0.227864583333`, OCS byte hit `0.217875346082`, EPS avg/max util `0.00014550974` / `0.00112466005333`, OCS avg/max util `7.32225206716e-05` / `0.000978884`, non-empty rounds `29.3333333333`, avg active edges `2.67346938776`, active lightpath seconds `1.31`, community ratio `0`.

### aggregation-k2

- `eps-ecmp`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000856091767578` / `0.00215556433333` / `0.00217266366667`, OCS assigned `0`, EPS fallback `512`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00036437408` / `0.01165997056`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000836550041015` / `0.002155753` / `0.00217510666667`, OCS assigned `36`, EPS fallback `476`, OCS hit `0.0703125`, OCS byte hit `0.0744050425386`, EPS avg/max util `0.00033854264` / `0.01099117952`, OCS avg/max util `0.00036255231746` / `0.00125178628571`, non-empty rounds `12`, avg active edges `0.489795918367`, active lightpath seconds `0.24`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000836550041015` / `0.002155753` / `0.00217510666667`, OCS assigned `36`, EPS fallback `476`, OCS hit `0.0703125`, OCS byte hit `0.0744050425386`, EPS avg/max util `0.00033854264` / `0.01099117952`, OCS avg/max util `0.00036255231746` / `0.00125178628571`, non-empty rounds `12`, avg active edges `0.489795918367`, active lightpath seconds `0.24`, community ratio `0`.

### aggregation-agg2

- `eps-ecmp`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000856152463542` / `0.00215560866667` / `0.00217266366667`, OCS assigned `0`, EPS fallback `512`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00036437408` / `0.00684756309333`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000851492142578` / `0.00215611366667` / `0.00217266366667`, OCS assigned `6.66666666667`, EPS fallback `505.333333333`, OCS hit `0.0130208333333`, OCS byte hit `0.0173561361345`, EPS avg/max util `0.00035826952` / `0.00673932245333`, OCS avg/max util `7.67642666667e-05` / `0.000684888533333`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000851492142578` / `0.00215611366667` / `0.00217266366667`, OCS assigned `6.66666666667`, EPS fallback `505.333333333`, OCS hit `0.0130208333333`, OCS byte hit `0.0173561361345`, EPS avg/max util `0.00035826952` / `0.00673932245333`, OCS avg/max util `7.67642666667e-05` / `0.000684888533333`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.

### aggregation-agg2-thetaf

- `eps-ecmp`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000856152463542` / `0.00215560866667` / `0.00217266366667`, OCS assigned `0`, EPS fallback `512`, OCS hit `0`, OCS byte hit `0`, EPS avg/max util `0.00036437408` / `0.00684756309333`, OCS avg/max util `0` / `0`, non-empty rounds `0`, avg active edges `0`, active lightpath seconds `0`, community ratio `0`.
- `ocs-volume`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000851492142578` / `0.00215611366667` / `0.00217266366667`, OCS assigned `6.66666666667`, EPS fallback `505.333333333`, OCS hit `0.0130208333333`, OCS byte hit `0.0173561361345`, EPS avg/max util `0.00035826952` / `0.00673932245333`, OCS avg/max util `7.67642666667e-05` / `0.000684888533333`, non-empty rounds `12`, avg active edges `0.244897959184`, active lightpath seconds `0.12`, community ratio `0`.
- `tl-ocs`: seeds `3`, completed `512/512`, throughput `503680000`, avg/p90/p95 FCT `0.000851492142578` / `0.00215611366667` / `0.00217266366667`, OCS assigned `6.66666666667`, EPS fallback `505.333333333`, OCS hit `0.0130208333333`, OCS byte hit `0.0173561361345`, EPS avg/max util `0.00035826952` / `0.00673932245333`, OCS avg/max util `8.04412952381e-05` / `0.000684888533333`, non-empty rounds `11.6666666667`, avg active edges `0.238095238095`, active lightpath seconds `0.116666666667`, community ratio `0`.

## Difference Readiness Notes

- Groups with observable TL-OCS versus OCS-Volume aggregate deltas: `uniform-main`, `community-main`, `aggregation-thetaf`, `community-k2`, `aggregation-agg2-thetaf`.
- Groups with weak or no TL-OCS versus OCS-Volume aggregate deltas: `aggregation-main`, `aggregation-k2`, `aggregation-agg2`.
- The comparison CSV records absolute and relative deltas with neutral `direction_hint` values only. It intentionally avoids significance or paper-conclusion language.

## Phase 15E Readiness

- Data quality and same-seed alignment are suitable for Phase 15E plotting preparation and visual inspection.
- Phase 15E should consume the processed CSVs generated by this script and keep raw CSVs immutable.
- Any statistical claims should wait for an explicit statistics phase; this report only prepares aggregate data.
