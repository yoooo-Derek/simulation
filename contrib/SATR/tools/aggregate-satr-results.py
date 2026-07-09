#!/usr/bin/env python3
import argparse
import csv
import math
import re
from pathlib import Path


CSV_FIELDS = [
    "trafficModel",
    "strategy",
    "offeredLoad",
    "workloadScale",
    "flowGenerationMode",
    "messageSizeBytes",
    "flowsPerActivePair",
    "trafficStartTime",
    "trafficStopTime",
    "simulationStopTime",
    "randomSeed",
    "runId",
    "electricalDataRate",
    "ocsDataRate",
    "electricalDelay",
    "ocsDelay",
    "eta",
    "alpha",
    "epsilon",
    "memsCount",
    "podPortLimitB",
    "circuitCapacityBps",
    "decoyBeta",
    "structuralBonus",
    "decoyHighActivity",
    "decoyLowActivity",
    "runtimeSeconds",
    "offeredBytes",
    "trafficBytes",
    "generatedFlows",
    "installableFlows",
    "unservedFlows",
    "installRatio",
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
    "logFile",
    "invalid",
    "invalidReason",
]

NUMERIC_FIELDS = {
    "offeredLoad",
    "workloadScale",
    "trafficStartTime",
    "trafficStopTime",
    "simulationStopTime",
    "eta",
    "alpha",
    "epsilon",
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
    "runtimeSeconds",
}

INTEGER_FIELDS = {
    "messageSizeBytes",
    "flowsPerActivePair",
    "randomSeed",
    "runId",
    "memsCount",
    "podPortLimitB",
    "circuitCapacityBps",
    "offeredBytes",
    "trafficBytes",
    "generatedFlows",
    "installableFlows",
    "unservedFlows",
    "installedFlows",
    "completedFlows",
    "incompleteFlows",
}

SUMMARY_RE = re.compile(r"(?:SATR|SMTRA) experiment: (?P<body>.*)")

REQUIRED_FIELDS = {
    "strategy",
    "offeredLoad",
    "unservedFlows",
    "fullyCompleted",
    "avgFctSeconds",
    "throughputGbps",
    "avgLinkUtilization",
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
        raise ValueError(f"missing SATR/SMTRA experiment summary: {log_file}")

    row = {"logFile": str(log_file)}
    for item in match.group("body").split(", "):
        if "=" not in item:
            raise ValueError(f"malformed summary item in {log_file}: {item}")
        key, value = item.split("=", 1)
        row[key] = parse_value(key, value)

    runtime_file = log_file.with_suffix(".runtime")
    row["runtimeSeconds"] = (
        parse_value("runtimeSeconds", runtime_file.read_text().strip())
        if runtime_file.exists()
        else ""
    )

    missing = sorted(REQUIRED_FIELDS - set(row))
    if missing:
        raise ValueError(f"missing fields in {log_file}: {', '.join(missing)}")

    reasons = []
    if row.get("ocsCoverageOk", "true") != "true":
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


def output_fields(rows):
    keys = {key for row in rows for key in row}
    tail = ["logFile", "invalid", "invalidReason"]
    fields = [field for field in CSV_FIELDS if field in keys and field not in tail]
    fields.extend(sorted(keys - set(fields) - set(tail)))
    fields.extend(field for field in tail if field in keys)
    return fields


def sort_key(row):
    try:
        offered = float(row["offeredLoad"])
    except ValueError:
        offered = math.inf
    return (offered, row["strategy"])


def main():
    parser = argparse.ArgumentParser(description="Aggregate SATR/SMTRA runner stdout logs into CSV.")
    parser.add_argument("log_dir", type=Path, help="Directory containing per-run .log files")
    parser.add_argument("--output", type=Path, required=True, help="Output CSV path")
    args = parser.parse_args()

    logs = sorted(args.log_dir.glob("*.log"))
    if not logs:
        raise SystemExit(f"no .log files found in {args.log_dir}")

    rows = [parse_log(log) for log in logs]
    rows.sort(key=sort_key)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=output_fields(rows), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)

    invalid_count = sum(1 for row in rows if row["invalid"] == "true")
    print(f"rows={len(rows)} invalid={invalid_count} output={args.output}")


if __name__ == "__main__":
    main()
