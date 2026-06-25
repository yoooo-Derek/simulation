#!/usr/bin/env python3
import argparse
import csv
import math
import re
from pathlib import Path


CSV_FIELDS = [
    "matrixMode",
    "observeTrafficModel",
    "testTrafficModel",
    "testPerturbationMode",
    "testPerturbationRatio",
    "phaseShift",
    "phaseShiftWrap",
    "communityRotationPattern",
    "observeMixA",
    "observeMixB",
    "observeMixAWeight",
    "testMixAWeight",
    "neighborWeight",
    "crossStageWeight",
    "backgroundWeight",
    "decoyBeta",
    "structuralBonus",
    "decoyHighActivity",
    "decoyLowActivity",
    "strategy",
    "structShortestMode",
    "offeredLoad",
    "workloadScale",
    "flowGenerationMode",
    "messageSizeBytes",
    "flowsPerActivePair",
    "electricalDataRate",
    "ocsDataRate",
    "memsCount",
    "podPortLimitB",
    "circuitCapacityBps",
    "runtimeSeconds",
    "observeBytes",
    "testBytes",
    "matrixAbsDiffBytes",
    "generatedFlows",
    "installableFlows",
    "unservedFlows",
    "installRatio",
    "ocsCoverageOk",
    "pathTypeCounts",
    "topRawPairs",
    "topSPairs",
    "topPsiPairs",
    "rawPsiTopKOverlap",
    "rawSTopKOverlap",
    "activeOcsEdges",
    "trafficFairEdges",
    "trafficFairSelectionOrder",
    "v8Edges",
    "edgeOverlapWithTopRaw",
    "edgeOverlapWithTopPsi",
    "opticalConnectionCount",
    "podPortUseMean",
    "podPortUseMax",
    "podPortUseMin",
    "topKCoveredPairCount",
    "topCoverage",
    "smdTop",
    "directStructuralWeightRatio",
    "oneHopPathFlows",
    "twoHopPathFlows",
    "multiHopPathFlows",
    "opticalDirectFlows",
    "nonOpticalFlows",
    "opticalDirectRatio",
    "nonOpticalTrafficRatio",
    "structuralMismatchMean",
    "structuralMismatchP95",
    "structuralMismatchMax",
    "strongFlowMismatchMean",
    "backgroundFlowMismatchMean",
    "pathSignatureCountMax",
    "pathSignatureCountP95",
    "pathSignatureCountMean",
    "uniquePathSignatureCount",
    "ocsEdgeFlowCountMean",
    "ocsEdgeFlowCountMax",
    "ocsEdgeFlowCountP95",
    "ocsEdgeFlowCountStd",
    "ocsEdgeStrongFlowCountMean",
    "ocsEdgeStrongFlowCountMax",
    "ocsEdgeBackgroundFlowCountMean",
    "ocsEdgeBackgroundFlowCountMax",
    "changedPathFlowCount",
    "changedPathFlowRatio",
    "changedStrongPathFlowCount",
    "changedBackgroundPathFlowCount",
    "edgeFlowImbalanceDeltaVsShortest",
    "pathConcentrationDeltaVsShortest",
    "equalShortestPathCountMean",
    "equalShortestPathCountMax",
    "equalShortestPathPairCount",
    "flowsWithMultipleShortestPaths",
    "flowsWithMultipleShortestPathsRatio",
    "avgPathHopCount",
    "maxPathHopCount",
    "installedFlows",
    "completedFlows",
    "incompleteFlows",
    "incompleteFlowDetails",
    "completionRatio",
    "fullyCompleted",
    "avgFctSeconds",
    "p90FctSeconds",
    "p95FctSeconds",
    "throughputGbps",
    "avgLinkUtilization",
    "ocsLinkUtilization",
    "electricalLinkUtilization",
    "p90LinkUtilization",
    "p95LinkUtilization",
    "p90OcsLinkUtilization",
    "p95OcsLinkUtilization",
    "p90ElectricalLinkUtilization",
    "p95ElectricalLinkUtilization",
    "logFile",
    "invalid",
    "invalidReason",
]

NUMERIC_FIELDS = {
    "offeredLoad",
    "workloadScale",
    "testPerturbationRatio",
    "observeMixAWeight",
    "testMixAWeight",
    "neighborWeight",
    "crossStageWeight",
    "backgroundWeight",
    "decoyBeta",
    "structuralBonus",
    "decoyHighActivity",
    "decoyLowActivity",
    "installRatio",
    "completionRatio",
    "avgFctSeconds",
    "p90FctSeconds",
    "p95FctSeconds",
    "throughputGbps",
    "avgLinkUtilization",
    "ocsLinkUtilization",
    "electricalLinkUtilization",
    "p90LinkUtilization",
    "p95LinkUtilization",
    "p90OcsLinkUtilization",
    "p95OcsLinkUtilization",
    "p90ElectricalLinkUtilization",
    "p95ElectricalLinkUtilization",
    "podPortUseMean",
    "topCoverage",
    "smdTop",
    "directStructuralWeightRatio",
    "opticalDirectRatio",
    "nonOpticalTrafficRatio",
    "structuralMismatchMean",
    "structuralMismatchP95",
    "structuralMismatchMax",
    "strongFlowMismatchMean",
    "backgroundFlowMismatchMean",
    "pathSignatureCountMax",
    "pathSignatureCountP95",
    "pathSignatureCountMean",
    "ocsEdgeFlowCountMean",
    "ocsEdgeFlowCountMax",
    "ocsEdgeFlowCountP95",
    "ocsEdgeFlowCountStd",
    "ocsEdgeStrongFlowCountMean",
    "ocsEdgeStrongFlowCountMax",
    "ocsEdgeBackgroundFlowCountMean",
    "ocsEdgeBackgroundFlowCountMax",
    "changedPathFlowRatio",
    "edgeFlowImbalanceDeltaVsShortest",
    "pathConcentrationDeltaVsShortest",
    "equalShortestPathCountMean",
    "equalShortestPathCountMax",
    "flowsWithMultipleShortestPathsRatio",
    "avgPathHopCount",
    "runtimeSeconds",
}

INTEGER_FIELDS = {
    "messageSizeBytes",
    "flowsPerActivePair",
    "phaseShift",
    "memsCount",
    "podPortLimitB",
    "circuitCapacityBps",
    "observeBytes",
    "testBytes",
    "matrixAbsDiffBytes",
    "generatedFlows",
    "installableFlows",
    "unservedFlows",
    "rawPsiTopKOverlap",
    "rawSTopKOverlap",
    "edgeOverlapWithTopRaw",
    "edgeOverlapWithTopPsi",
    "opticalConnectionCount",
    "podPortUseMax",
    "podPortUseMin",
    "topKCoveredPairCount",
    "oneHopPathFlows",
    "twoHopPathFlows",
    "multiHopPathFlows",
    "opticalDirectFlows",
    "nonOpticalFlows",
    "uniquePathSignatureCount",
    "changedPathFlowCount",
    "changedStrongPathFlowCount",
    "changedBackgroundPathFlowCount",
    "equalShortestPathPairCount",
    "flowsWithMultipleShortestPaths",
    "maxPathHopCount",
    "installedFlows",
    "completedFlows",
    "incompleteFlows",
}

SUMMARY_RE = re.compile(r"SMTRA experiment: (?P<body>.*)")

OPTIONAL_FIELD_DEFAULTS = {
    "phaseShift": "",
    "phaseShiftWrap": "",
    "communityRotationPattern": "",
    "observeMixA": "",
    "observeMixB": "",
    "observeMixAWeight": "",
    "testMixAWeight": "",
    "neighborWeight": "",
    "crossStageWeight": "",
    "backgroundWeight": "",
    "decoyBeta": "",
    "structuralBonus": "",
    "decoyHighActivity": "",
    "decoyLowActivity": "",
    "structShortestMode": "",
    "topRawPairs": "",
    "topSPairs": "",
    "topPsiPairs": "",
    "rawPsiTopKOverlap": "",
    "rawSTopKOverlap": "",
    "activeOcsEdges": "",
    "trafficFairEdges": "",
    "trafficFairSelectionOrder": "",
    "v8Edges": "",
    "edgeOverlapWithTopRaw": "",
    "edgeOverlapWithTopPsi": "",
    "opticalConnectionCount": "NA",
    "podPortUseMean": "NA",
    "podPortUseMax": "NA",
    "podPortUseMin": "NA",
    "topKCoveredPairCount": "NA",
    "topCoverage": "NA",
    "smdTop": "NA",
    "directStructuralWeightRatio": "NA",
    "opticalDirectFlows": "NA",
    "nonOpticalFlows": "NA",
    "opticalDirectRatio": "NA",
    "nonOpticalTrafficRatio": "NA",
    "structuralMismatchMean": "NA",
    "structuralMismatchP95": "NA",
    "structuralMismatchMax": "NA",
    "strongFlowMismatchMean": "NA",
    "backgroundFlowMismatchMean": "NA",
    "pathSignatureCountMax": "NA",
    "pathSignatureCountP95": "NA",
    "pathSignatureCountMean": "NA",
    "uniquePathSignatureCount": "NA",
    "ocsEdgeFlowCountMean": "NA",
    "ocsEdgeFlowCountMax": "NA",
    "ocsEdgeFlowCountP95": "NA",
    "ocsEdgeFlowCountStd": "NA",
    "ocsEdgeStrongFlowCountMean": "NA",
    "ocsEdgeStrongFlowCountMax": "NA",
    "ocsEdgeBackgroundFlowCountMean": "NA",
    "ocsEdgeBackgroundFlowCountMax": "NA",
    "changedPathFlowCount": "NA",
    "changedPathFlowRatio": "NA",
    "changedStrongPathFlowCount": "NA",
    "changedBackgroundPathFlowCount": "NA",
    "edgeFlowImbalanceDeltaVsShortest": "NA",
    "pathConcentrationDeltaVsShortest": "NA",
    "equalShortestPathCountMean": "NA",
    "equalShortestPathCountMax": "NA",
    "equalShortestPathPairCount": "NA",
    "flowsWithMultipleShortestPaths": "NA",
    "flowsWithMultipleShortestPathsRatio": "NA",
    "p90FctSeconds": "NA",
    "p95FctSeconds": "NA",
    "ocsLinkUtilization": "NA",
    "electricalLinkUtilization": "NA",
    "p90LinkUtilization": "NA",
    "p95LinkUtilization": "NA",
    "p90OcsLinkUtilization": "NA",
    "p95OcsLinkUtilization": "NA",
    "p90ElectricalLinkUtilization": "NA",
    "p95ElectricalLinkUtilization": "NA",
    "incompleteFlowDetails": "",
}


def parse_value(key, value):
    if value == "NA":
        return value
    if key in INTEGER_FIELDS:
        return str(int(value))
    if key in NUMERIC_FIELDS:
        parsed = float(value)
        if math.isnan(parsed):
            return "NA"
        if math.isinf(parsed):
            return "inf" if parsed > 0 else "-inf"
        return f"{parsed:.12g}"
    return value


def parse_log(log_file):
    text = log_file.read_text(encoding="utf-8", errors="replace")
    match = SUMMARY_RE.search(text)
    if match is None:
        raise ValueError(f"missing SMTRA experiment summary: {log_file}")

    row = {"logFile": str(log_file)}
    for item in match.group("body").split(", "):
        if "=" not in item:
            raise ValueError(f"malformed summary item in {log_file}: {item}")
        key, value = item.split("=", 1)
        row[key] = parse_value(key, value)

    for key, value in OPTIONAL_FIELD_DEFAULTS.items():
        row.setdefault(key, value)

    runtime_file = log_file.with_suffix(".runtime")
    if runtime_file.exists():
        row["runtimeSeconds"] = parse_value("runtimeSeconds", runtime_file.read_text().strip())
    else:
        row["runtimeSeconds"] = ""

    missing = [field for field in CSV_FIELDS[:-2] if field not in row]
    if missing:
        raise ValueError(f"missing fields in {log_file}: {', '.join(missing)}")

    reasons = []
    if row["ocsCoverageOk"] != "true":
        reasons.append("ocsCoverageOk!=true")
    if int(row["unservedFlows"]) != 0:
        reasons.append("unservedFlows!=0")
    if row["fullyCompleted"] != "true":
        reasons.append("fullyCompleted!=true")
    try:
        avg_fct = float(row["avgFctSeconds"])
        if math.isnan(avg_fct) or math.isinf(avg_fct):
            reasons.append("avgFctSeconds_invalid")
    except ValueError:
        reasons.append("avgFctSeconds_invalid")

    row["invalid"] = "true" if reasons else "false"
    row["invalidReason"] = ";".join(reasons)
    return row


COMPARISON_COLUMNS = [
    ("offered_load", "offeredLoad"),
    ("strategy", "strategy"),
    ("struct_mode", "structShortestMode"),
    ("avg_fct", "avgFctSeconds"),
    ("p90_fct", "p90FctSeconds"),
    ("p95_fct", "p95FctSeconds"),
    ("throughput", "throughputGbps"),
    ("avg_link_util", "avgLinkUtilization"),
    ("ocs_link_util", "ocsLinkUtilization"),
    ("electrical_link_util", "electricalLinkUtilization"),
    ("p95_link_util", "p95LinkUtilization"),
    ("p95_ocs_util", "p95OcsLinkUtilization"),
    ("top_coverage", "topCoverage"),
    ("smd_top", "smdTop"),
    ("direct_structural_weight_ratio", "directStructuralWeightRatio"),
    ("optical_direct_ratio", "opticalDirectRatio"),
    ("struct_mismatch_mean", "structuralMismatchMean"),
    ("struct_mismatch_p95", "structuralMismatchP95"),
    ("changed_path_ratio", "changedPathFlowRatio"),
    ("edge_imb_delta", "edgeFlowImbalanceDeltaVsShortest"),
    ("path_conc_delta", "pathConcentrationDeltaVsShortest"),
    ("multi_shortest_ratio", "flowsWithMultipleShortestPathsRatio"),
    ("avg_hop_count", "avgPathHopCount"),
]


def sort_key(row):
    try:
        offered = float(row["offeredLoad"])
    except ValueError:
        offered = math.inf
    return (offered, row["strategy"])


def print_comparison_table(rows):
    table = []
    headers = [header for header, _ in COMPARISON_COLUMNS]
    table.append(headers)
    for row in sorted(rows, key=sort_key):
        table.append([row.get(field, "NA") or "NA" for _, field in COMPARISON_COLUMNS])

    widths = [0] * len(headers)
    for line in table:
        for index, value in enumerate(line):
            widths[index] = max(widths[index], len(value))

    print("comparison:")
    for line_index, line in enumerate(table):
        print("  " + "  ".join(value.ljust(widths[index]) for index, value in enumerate(line)))
        if line_index == 0:
            print("  " + "  ".join("-" * widths[index] for index in range(len(widths))))


def main():
    parser = argparse.ArgumentParser(description="Aggregate smtra-runner stdout logs into CSV.")
    parser.add_argument("log_dir", type=Path, help="Directory containing per-run .log files")
    parser.add_argument("--output", type=Path, required=True, help="Output CSV path")
    args = parser.parse_args()

    logs = sorted(args.log_dir.glob("*.log"))
    if not logs:
        raise SystemExit(f"no .log files found in {args.log_dir}")

    rows = [parse_log(log) for log in logs]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=CSV_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    invalid_count = sum(1 for row in rows if row["invalid"] == "true")
    print(f"rows={len(rows)} invalid={invalid_count} output={args.output}")
    print_comparison_table(rows)


if __name__ == "__main__":
    main()
