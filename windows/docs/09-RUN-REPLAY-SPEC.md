# Runs, persistence and replay

SQLite with migration versioning from first implementation. One writer, foreign keys,
WAL, bounded batched transactions and transactional run creation. Data lives in local
application data outside synced folders. Tables: migrations, runs, samples, events,
experiment_executions, phase_runs, artifacts. Unique (run_id, sequence), sample index
(run_id, elapsed_us), and foreign keys establish ordering and ownership.

Run metadata holds origin, time anchors, inventory/capabilities snapshots, exact workload,
experiment link, engine name/version/hash/argv, mapping/analyzer/simulator versions,
seed where relevant, outcome, abort reason, summaries and artifact hashes.
Raw XML is bounded and retained separately with metadata. Do not serialize API secrets.

Replay samples store full telemetry + normalized flow + analyzer/safety events +
phase/status. They are self-contained snapshots so seeking does not depend on playing
earlier frames. Store measured data unchanged; recorded presentation is a reader mode.
RECORDED + SIMULATED origin must remain obvious for synthetic recordings.

Play/pause, previous/next sample, previous/next event, scrub and 0.25/0.5/1/2/4× speed.
Seek selects latest sample at or before requested time; no future metric interpolation.
Particle interpolation uses selected sample and deterministic time offset only.
At gaps, show gap and stop interpolation. At end pause. Invalid/unknown schema version,
broken references or corruption rejects import without damaging existing runs.
Only migrate copies; keep original artifact.

Mark active runs interrupted on startup after crash. Checkpoint/WAL recovery must be
tested. Recording overflow is explicit; if persistence cannot retain reliable evidence,
abort the active workload and preserve the partial run. DiskSpd completion without
durable final metadata cannot be reported as a fully saved run.

Compare two compatible metric scopes/units/windows with absolute and relative delta,
sample counts and settings. No division by zero; no averaging percentiles from runs.
Incompatible comparisons explain the mismatch. Completed-run p95 is not reconstructed
from per-second averages. History offers export and explicit deletion, never silent eviction.
