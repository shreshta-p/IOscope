"""Validate the Phase 0 wire baseline. No hardware access."""
from copy import deepcopy
import json
import math
import argparse
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker
from jsonschema.exceptions import ValidationError
from referencing import Registry, Resource

ROOT = Path(__file__).resolve().parents[1]
CONTRACTS = ROOT / "contracts"
if "date-time" not in FormatChecker.checkers:
    raise RuntimeError("Install tools/requirements.txt: date-time validation is required")


def read(path):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError(f"Duplicate JSON key: {key}")
            result[key] = value
        return result

    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)


schemas = [read(path) for path in (CONTRACTS / "v1").glob("*.schema.json")]
registry = Registry().with_resources(
    (schema["$id"], Resource.from_contents(schema)) for schema in schemas
)
validators = {}
for schema in schemas:
    Draft202012Validator.check_schema(schema)
    if schema.get("title") != "IOscope canonical domain definitions":
        validators[schema["title"]] = Draft202012Validator(
            schema, registry=registry, format_checker=FormatChecker()
        )
catalog = {item["metricId"]: item for item in read(
    CONTRACTS / "metric-catalog.v1.json"
)["metrics"]}
mapping = read(CONTRACTS / "flow-semantics.v1.json")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def semantic(value):
    if isinstance(value, list):
        for item in value:
            semantic(item)
        return
    if not isinstance(value, dict):
        if isinstance(value, float):
            require(math.isfinite(value), "nonfinite number")
        return
    if "metricId" in value and "value" in value:
        metric = catalog.get(value["metricId"])
        require(metric is not None, "unknown metric")
        require(value["unit"] == metric["unit"], "metric unit mismatch")
        number = value["value"]
        if number is not None:
            require(math.isfinite(number), "nonfinite metric")
            require(number >= metric["min"], "metric below minimum")
            require(metric["max"] is None or number <= metric["max"],
                    "metric above maximum")
        require(value["provenance"] != "conceptual", "physical metric is conceptual")
    if "measurements" in value:
        keys = [(m["deviceId"], m["metricId"], m["scope"])
                for m in value["measurements"]]
        require(len(keys) == len(set(keys)), "duplicate metric key")
        allowed = {"simulated"} if value["origin"] == "simulated" else {"measured", "derived"}
        require(all(m["provenance"] in allowed for m in value["measurements"]),
                "origin/provenance mismatch")
    if "telemetry" in value and "flow" in value:
        telemetry, flow = value["telemetry"], value["flow"]
        require(telemetry["sequence"] == flow["sequence"], "sequence mismatch")
        require(telemetry["elapsedUs"] == flow["elapsedUs"], "time mismatch")
        require(value["runId"] == telemetry["runId"] ==
                value["workloadStatus"]["runId"], "run identity mismatch")
        available = {(telemetry["sequence"], m["deviceId"], m["metricId"])
                     for m in telemetry["measurements"]
                     if m["status"] == "available" and m["ageMs"] <= 3000}
        for path in flow["paths"]:
            if path["status"] == "active":
                require(bool(path["evidence"]), "active path lacks evidence")
            for evidence in path["evidence"]:
                require((evidence["sequence"], evidence["deviceId"],
                         evidence["metricId"]) in available, "invalid flow evidence")
        for event in value["analyzerEvents"] + value["safetyEvents"]:
            require(event["runId"] == value["runId"], "event run mismatch")
        for path in flow["paths"]:
            spec = next((p for p in mapping["paths"]
                         if p["pathId"] == path["pathId"]), None)
            if spec and path["status"] != "unknown":
                source = next((m for m in telemetry["measurements"]
                               if m["metricId"] == spec["metricId"]), None)
                require(source is not None and source["value"] is not None,
                        "mapped flow missing source")
                scale = mapping["bandwidthScaleBytesPerSecond"]
                expected = math.log2(1 + min(source["value"], scale) / 1048576) / math.log2(1 + scale / 1048576)
                require(math.isclose(path["activity"], expected, abs_tol=1e-12),
                        "flow activity differs from canonical mapping")
    if "mappingVersion" in value and "paths" in value:
        for path in value["paths"]:
            require(path["status"] == "active" or path["activity"] == 0,
                    "inactive flow has activity")
    if "outcome" in value:
        terminal = value["outcome"] != "running"
        require((value["endedAt"] is not None) == terminal, "terminal timestamp")
        if value["outcome"] not in {"running", "completed"}:
            require(bool(value["abortReason"]), "abnormal outcome lacks reason")
        if value["origin"] == "simulated":
            require(value["seed"] is not None and value["simulatorVersion"] is not None,
                    "simulator identity missing")
    for child in value.values():
        semantic(child)


def validate(name, value):
    validators[name].validate(value)
    semantic(value)


fixtures = {name: read(CONTRACTS / "fixtures" / f"{name}.json")
            for name in validators}
for name, value in fixtures.items():
    validate(name, value)

negative_count = 0


def rejected(name, mutate):
    global negative_count
    value = deepcopy(fixtures[name])
    mutate(value)
    try:
        validate(name, value)
    except (ValueError, ValidationError):
        negative_count += 1
        return
    raise AssertionError(f"Invalid {name} was accepted")


rejected("WorkloadDefinition", lambda x: x.update(queueDepth=33))
rejected("WorkloadDefinition", lambda x: x.update(queueDepth=0))
rejected("WorkloadDefinition", lambda x: x.update(readPercent=101))
rejected("WorkloadDefinition", lambda x: x.update(workingSetBytes=4294967297))
rejected("WorkloadDefinition", lambda x: x.update(durationSeconds=61))
rejected("WorkloadDefinition", lambda x: x.update(blockBytes=8192))
rejected("WorkloadDefinition", lambda x: x.update(command="arbitrary"))
rejected("TelemetryFrame", lambda x: x.update(schemaVersion="2.0.0"))
rejected("TelemetryFrame", lambda x: x.update(capturedAt="not-a-date"))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(status="unavailable"))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(value=None))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(unit="ms"))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(value=float("nan")))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(value=float("inf")))
rejected("TelemetryFrame", lambda x: x["measurements"][0].update(value=-1))
rejected("TelemetryFrame", lambda x: x.update(origin="live"))
rejected("TelemetryFrame", lambda x: x["measurements"].append(x["measurements"][0]))
rejected("PresentationContext", lambda x: x.update(mode="LIVE"))
rejected("PresentationContext", lambda x: x.update(runId=None))
rejected("ReplaySample", lambda x: x["flow"].update(sequence=4))
rejected("ReplaySample", lambda x: x["flow"]["paths"][0].update(evidence=[]))
rejected("ReplaySample", lambda x: x["telemetry"]["measurements"][0].update(ageMs=3001))
rejected("ReplaySample", lambda x: x.update(runId="different-run"))
rejected("ReplaySample", lambda x: x["flow"]["paths"][0].update(activity=0.9))
rejected("RunMetadata", lambda x: x.update(endedAt=None))
rejected("RunMetadata", lambda x: x.update(seed=None))
rejected("RunMetadata", lambda x: x.update(outcome="aborted"))

unavailable = deepcopy(fixtures["TelemetryFrame"])
unavailable["measurements"][0].update(
    status="unavailable", value=None, reason="Sensor unavailable")
validate("TelemetryFrame", unavailable)
print(f"PASS: {len(schemas)} schemas, {len(fixtures)} root fixtures, "
      f"1 unavailable fixture, {negative_count} rejected invalid mutations")
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--recording', type=Path)
arguments = parser.parse_args()
if arguments.recording:
    recording = read(arguments.recording)
    validate('Recording', recording)
    print(f"PASS: exported recording; {len(recording['samples'])} snapshots")
