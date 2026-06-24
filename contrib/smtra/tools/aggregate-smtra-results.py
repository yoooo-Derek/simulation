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
    "oneHopPathFlows",
    "twoHopPathFlows",
    "multiHopPathFlows",
    "avgPathHopCount",
    "maxPathHopCount",
    "installedFlows",
    "completedFlows",
    "incompleteFlows",
    "completionRatio",
    "fullyCompleted",
    "avgFctSeconds",
    "throughputGbps",
    "avgLinkUtilization",
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
    "throughputGbps",
    "avgLinkUtilization",
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
    "oneHopPathFlows",
    "twoHopPathFlows",
    "multiHopPathFlows",
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
}


def parse_value(key, value):
    if key in INTEGER_FIELDS:
        return str(int(value))
    if key in NUMERIC_FIELDS:
        parsed = float(value)
        if math.isnan(parsed):
            return "NaN"
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


if __name__ == "__main__":
    main()
