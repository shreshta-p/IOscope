# 3D digital twin

Fixed logical topology: CPU, system RAM, NVMe, GPU, VRAM, motherboard/interconnect.
Physical positioning is an illustrative laptop board, not an ASUS CAD reproduction.
Device inventory binds logical slots by stable device ID; unknown placement is labeled.
Use purpose-built geometry first; provenance-reviewed models can replace it later.

Orbit/pan/zoom have bounded distance and polar angle, no auto-rotation. Selection
uses pointer movement thresholds so dragging cannot select. Focus/reset ease over
350ms unless reduced motion. Isolate preserves a route back and dimmed context.
Keyboard component list drives the same selection state.

Data-path view includes Application → Filesystem → Page Cache → Block I/O →
Storage Driver → PCIe/storage interconnect → NVMe, all conceptual unless measured.
GPU route is NVMe → RAM → PCIe → VRAM → GPU. Do not infer cache hits, direct DMA,
GPU waiting or per-stage duration from system disk traffic.

Canonical semantic mapping is contracts/flow-semantics.v1.json. Paths consume
normalized metrics with evidence references. Missing/stale activity suppresses
particles and marks the path unknown. Replay supplies timeline time, never wall time.
Stable particle positions derive from run seed, path ID and replay elapsed time.
No individual request IDs, read/write queue counts or GPU transfers are invented.

NVMe inspector separates configured outstanding limit, observed system queue average,
and completed-run latency. Use “Aggregated queue representation”; an unknown queue
has no filled tokens. Fractional average queue length is not rounded into exact requests.
Initial caps: 256 instanced particles, 150 draw calls, 300k triangles, pixel ratio 1.5.
Adapt quality before reducing metric fidelity. Performance targets are unverified
until Phase 3; see 17-PERFORMANCE-BUDGET.md.
