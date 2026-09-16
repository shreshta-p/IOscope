# Performance budget

All numbers below are proposed acceptance targets, not measurements.

| Budget | Initial target / method |
|---|---|
| Sampling | 1Hz; adapter p95 <250ms, no accumulated polling backlog |
| Collector overhead | <1% total CPU normalized across logical CPUs at idle, <150MiB private memory |
| Transport | <64KiB/s typical, message <=256KiB, queue <=1MiB per client |
| Recording | average <128KiB/s; flush batch <=1s; memory queue <=8MiB |
| Scene | 60fps target at 1440×900; p95 frame <=20ms over 5min |
| Reduced quality | >=30fps; no change to metric meaning |
| Responsiveness | selection/stop acknowledgment p95 <=100/250ms locally |
| Stability | 30min replay loop; retained heap growth <10% after warmup/GC |
| Distortion | report paired baseline; investigate >3% throughput or latency shift |

Full-quality 3D competes with GPU workloads. Provide measurement mode: static/minimal
scene, collectors and recorder retained. Record UI mode in run metadata. Do not claim
GPU timing is independent of visualization. Agent controls safety deadlines independently.

Drop/coalesce intermediate UI frames under backpressure with gap metadata; never silently
discard recorded evidence. Batch immutable samples without React rerender per particle.
No FPS claim until profiling on target hardware; record resolution, power mode,
display GPU, driver versions and trial variance.
