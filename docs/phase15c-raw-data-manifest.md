# Phase 15C Raw Data Manifest

This manifest records the expected Phase 15C finite multi-cycle raw data files. It is a run-status document, not a result table for paper conclusions.

- Commit: `c2e0c22 Add V5 alignment and calibration reports`
- Expected runs: 72
- Completed runs: 72
- Failed runs: 0
- Missing runs: 0
- Result CSV files remain under `results/raw` and are not tracked by git.

## Summary By Group

- `uniform-main`: uniform main, k=1, thetaF=0, agg=1; 9/9 runs passed.
- `community-main`: community-local main, k=1, thetaF=0, agg=1; 9/9 runs passed.
- `aggregation-main`: parameter-aggregation main, k=1, thetaF=0, agg=1; 9/9 runs passed.
- `aggregation-thetaf`: parameter-aggregation thetaF sensitivity, k=1, thetaF=50000, agg=1; 9/9 runs passed.
- `community-k2`: community-local k=2 sensitivity, k=2, thetaF=0, agg=1; 9/9 runs passed.
- `aggregation-k2`: parameter-aggregation k=2 sensitivity, k=2, thetaF=0, agg=1; 9/9 runs passed.
- `aggregation-agg2`: parameter-aggregation aggregatorCount=2 sensitivity, k=1, thetaF=0, agg=2; 9/9 runs passed.
- `aggregation-agg2-thetaf`: parameter-aggregation aggregatorCount=2 + thetaF sensitivity, k=1, thetaF=50000, agg=2; 9/9 runs passed.

## Run Inventory

Columns: group, scheme, seed, k, thetaF, aggregatorCount, status, completed/total, summary CSV, per-flow CSV, notes.

```csv
group,scheme,seed,k,thetaF,aggregatorCount,status,completed_total,summary_csv,flows_csv,notes
uniform-main,eps-ecmp,1401,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-eps-ecmp-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-eps-ecmp-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,ocs-volume,1401,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-ocs-volume-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-ocs-volume-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,tl-ocs,1401,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-tl-ocs-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-tl-ocs-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,eps-ecmp,1417,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-eps-ecmp-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-eps-ecmp-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,ocs-volume,1417,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-ocs-volume-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-ocs-volume-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,tl-ocs,1417,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-tl-ocs-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-tl-ocs-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,eps-ecmp,1433,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-eps-ecmp-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-eps-ecmp-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,ocs-volume,1433,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-ocs-volume-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-ocs-volume-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
uniform-main,tl-ocs,1433,1,0,1,passed,256/256,results/raw/phase15c-uniform-main-tl-ocs-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-uniform-main-tl-ocs-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,eps-ecmp,1401,1,0,1,passed,256/256,results/raw/phase15c-community-main-eps-ecmp-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-eps-ecmp-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,ocs-volume,1401,1,0,1,passed,256/256,results/raw/phase15c-community-main-ocs-volume-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-ocs-volume-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,tl-ocs,1401,1,0,1,passed,256/256,results/raw/phase15c-community-main-tl-ocs-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-tl-ocs-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,eps-ecmp,1417,1,0,1,passed,256/256,results/raw/phase15c-community-main-eps-ecmp-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-eps-ecmp-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,ocs-volume,1417,1,0,1,passed,256/256,results/raw/phase15c-community-main-ocs-volume-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-ocs-volume-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,tl-ocs,1417,1,0,1,passed,256/256,results/raw/phase15c-community-main-tl-ocs-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-tl-ocs-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,eps-ecmp,1433,1,0,1,passed,256/256,results/raw/phase15c-community-main-eps-ecmp-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-eps-ecmp-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,ocs-volume,1433,1,0,1,passed,256/256,results/raw/phase15c-community-main-ocs-volume-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-ocs-volume-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
community-main,tl-ocs,1433,1,0,1,passed,256/256,results/raw/phase15c-community-main-tl-ocs-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-community-main-tl-ocs-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,eps-ecmp,1401,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-eps-ecmp-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-eps-ecmp-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,ocs-volume,1401,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-ocs-volume-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-ocs-volume-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,tl-ocs,1401,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-tl-ocs-seed1401-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-tl-ocs-seed1401-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,eps-ecmp,1417,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-eps-ecmp-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-eps-ecmp-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,ocs-volume,1417,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-ocs-volume-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-ocs-volume-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,tl-ocs,1417,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-tl-ocs-seed1417-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-tl-ocs-seed1417-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,eps-ecmp,1433,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-eps-ecmp-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-eps-ecmp-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,ocs-volume,1433,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-ocs-volume-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-ocs-volume-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-main,tl-ocs,1433,1,0,1,passed,512/512,results/raw/phase15c-aggregation-main-tl-ocs-seed1433-k1-thetaF0-agg1.csv,results/raw/phase15c-aggregation-main-tl-ocs-seed1433-k1-thetaF0-agg1-flows.csv,quality_passed
aggregation-thetaf,eps-ecmp,1401,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1401-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1401-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,ocs-volume,1401,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1401-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1401-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,tl-ocs,1401,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1401-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1401-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,eps-ecmp,1417,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1417-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1417-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,ocs-volume,1417,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1417-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1417-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,tl-ocs,1417,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1417-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1417-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,eps-ecmp,1433,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1433-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-eps-ecmp-seed1433-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,ocs-volume,1433,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1433-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-ocs-volume-seed1433-k1-thetaF50000-agg1-flows.csv,quality_passed
aggregation-thetaf,tl-ocs,1433,1,50000,1,passed,512/512,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1433-k1-thetaF50000-agg1.csv,results/raw/phase15c-aggregation-thetaf-tl-ocs-seed1433-k1-thetaF50000-agg1-flows.csv,quality_passed
community-k2,eps-ecmp,1401,2,0,1,passed,256/256,results/raw/phase15c-community-k2-eps-ecmp-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-eps-ecmp-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,ocs-volume,1401,2,0,1,passed,256/256,results/raw/phase15c-community-k2-ocs-volume-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-ocs-volume-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,tl-ocs,1401,2,0,1,passed,256/256,results/raw/phase15c-community-k2-tl-ocs-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-tl-ocs-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,eps-ecmp,1417,2,0,1,passed,256/256,results/raw/phase15c-community-k2-eps-ecmp-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-eps-ecmp-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,ocs-volume,1417,2,0,1,passed,256/256,results/raw/phase15c-community-k2-ocs-volume-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-ocs-volume-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,tl-ocs,1417,2,0,1,passed,256/256,results/raw/phase15c-community-k2-tl-ocs-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-tl-ocs-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,eps-ecmp,1433,2,0,1,passed,256/256,results/raw/phase15c-community-k2-eps-ecmp-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-eps-ecmp-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,ocs-volume,1433,2,0,1,passed,256/256,results/raw/phase15c-community-k2-ocs-volume-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-ocs-volume-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
community-k2,tl-ocs,1433,2,0,1,passed,256/256,results/raw/phase15c-community-k2-tl-ocs-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-community-k2-tl-ocs-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,eps-ecmp,1401,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,ocs-volume,1401,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-ocs-volume-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-ocs-volume-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,tl-ocs,1401,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-tl-ocs-seed1401-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-tl-ocs-seed1401-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,eps-ecmp,1417,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,ocs-volume,1417,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-ocs-volume-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-ocs-volume-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,tl-ocs,1417,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-tl-ocs-seed1417-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-tl-ocs-seed1417-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,eps-ecmp,1433,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-eps-ecmp-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,ocs-volume,1433,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-ocs-volume-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-ocs-volume-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-k2,tl-ocs,1433,2,0,1,passed,512/512,results/raw/phase15c-aggregation-k2-tl-ocs-seed1433-k2-thetaF0-agg1.csv,results/raw/phase15c-aggregation-k2-tl-ocs-seed1433-k2-thetaF0-agg1-flows.csv,quality_passed
aggregation-agg2,eps-ecmp,1401,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1401-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1401-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,ocs-volume,1401,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1401-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1401-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,tl-ocs,1401,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1401-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1401-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,eps-ecmp,1417,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1417-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1417-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,ocs-volume,1417,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1417-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1417-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,tl-ocs,1417,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1417-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1417-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,eps-ecmp,1433,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1433-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-eps-ecmp-seed1433-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,ocs-volume,1433,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1433-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-ocs-volume-seed1433-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2,tl-ocs,1433,1,0,2,passed,512/512,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1433-k1-thetaF0-agg2.csv,results/raw/phase15c-aggregation-agg2-tl-ocs-seed1433-k1-thetaF0-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,eps-ecmp,1401,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1401-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1401-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,ocs-volume,1401,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1401-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1401-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,tl-ocs,1401,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1401-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1401-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,eps-ecmp,1417,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1417-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1417-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,ocs-volume,1417,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1417-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1417-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,tl-ocs,1417,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1417-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1417-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,eps-ecmp,1433,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1433-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-eps-ecmp-seed1433-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,ocs-volume,1433,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1433-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-ocs-volume-seed1433-k1-thetaF50000-agg2-flows.csv,quality_passed
aggregation-agg2-thetaf,tl-ocs,1433,1,50000,2,passed,512/512,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1433-k1-thetaF50000-agg2.csv,results/raw/phase15c-aggregation-agg2-thetaf-tl-ocs-seed1433-k1-thetaF50000-agg2-flows.csv,quality_passed
```

## Quality Notes

- Summary and per-flow CSV files passed header/value column-count checks.
- Per-flow row counts matched summary `total_flows`.
- Same-seed flow sequence alignment passed across `eps-ecmp`, `ocs-volume`, and `tl-ocs` for every group.
- EPS-only runs had zero OCS assignment, hit rate, and utilization fields.
- All path types were `eps` or `ocs`.
- No failed, missing, or incomplete-flow runs were observed in this batch.
