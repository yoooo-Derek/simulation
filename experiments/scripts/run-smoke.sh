#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

./ns3 run "tl-ocs-runner --numTors=4 --serversPerTor=2 --spines=1 --observerWindow=0.001 --ocsPeriod=0.005 --stopTime=0.05 --experimentName=phase6-algorithm --schemeName=tl-ocs-smoke --trafficPattern=community-local --outputDir=results/raw --summaryFile=phase6-algorithm.csv --overwrite=true --enableEpsTopology=true --enableTrainingTraffic=true --enableTrafficObserver=true --enableAlgorithmSmoke=true --observerDumpMatrix=true --numFlows=8 --flowSizeBytes=100000 --flowStartInterval=0.001 --communityCount=2 --thetaF=0 --eta=1.0 --alpha=0.5 --opticalPortsPerTor=1"
