# Portfolio demonstrations

## Queue depth (target 1–2 minute narrative)
Start with a clearly labeled live or simulated source. Show fixed random-read settings,
QD1, then serial QD steps. Highlight throughput and completed-phase latency alongside
system queue, open aggregated queue inspector, explain plateau possibilities including
the intensity rate cap. Save and replay exact phase boundaries and evidence.
A live QD control does not mutate a running DiskSpd process.

## Model-like loading
Show preparation, storage read, RAM allocation, H2D, VRAM, optional checksum compute,
release and total time. Label conceptual links and measured phase timings separately.
Replay changes in state rather than claiming individual bytes/requests were observed.
Unsupported CUDA demonstrates capability fallback, never fake live transfers.

Final README adds real screenshots/GIFs only after working features. Include architecture,
safe setup, version details, measured overhead, replay reproducibility and limitations.
No “hyper-realistic” or “real-time” performance claim without supporting artifact.
