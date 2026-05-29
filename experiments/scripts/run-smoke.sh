#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

./ns3 run "tl-ocs-runner --numTors=4 --serversPerTor=2 --spines=1 --observerWindow=0.001 --ocsPeriod=0.005 --stopTime=0.05 --experimentName=phase5-observer --schemeName=eps-smoke --trafficPattern=uniform --outputDir=results/raw --summaryFile=phase5-observer.csv --overwrite=true --enableEpsTopology=true --enableTrainingTraffic=true --enableTrafficObserver=true --observerDumpMatrix=true --numFlows=4 --flowSizeBytes=100000 --flowStartInterval=0.001"
