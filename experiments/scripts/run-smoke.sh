#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."

./ns3 run "tl-ocs-runner --numTors=2 --serversPerTor=1 --spines=1 --observerWindow=0.001 --ocsPeriod=0.005 --stopTime=0.05 --experimentName=phase3-tcp-smoke --schemeName=eps-smoke --trafficPattern=single-tcp --outputDir=results/raw --summaryFile=phase3-summary.csv --overwrite=true --enableEpsTopology=true --enableTcpSmoke=true --tcpFlowBytes=1000000"
