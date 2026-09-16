# Learn

Each topic has four short sections: what, why, see in system, try yourself.
No textbook-sized pages. Component links update the same selection state as the twin.

| Topic | Core distinction | Highlight / experiment |
|---|---|---|
| Queue depth | outstanding work; configured cap differs from observed average | NVMe / QD |
| IOPS | completed operations per second | NVMe / block sweep |
| Throughput | bytes per second; block × operations only for matching scopes | storage path / block sweep |
| Latency | elapsed completion time; average differs from tail | NVMe / QD |
| Block size | bytes per operation | NVMe / block sweep |
| Sequential/random | access order, not inherently fast/slow | storage path / presets |
| Buffered I/O | Windows file data cache participates | page cache / cache comparison |
| Page cache | conceptual RAM-backed cache; hits not directly observed | RAM / first/repeated |
| Working set | generated dataset size, distinct from process working set | RAM + NVMe / cache |
| GPU VRAM | device memory allocation, not compute activity | VRAM / pipeline |
| Host-to-device | explicit RAM-to-device copy | PCIe / pipeline |
| PCIe | shared interconnect; path animation is conceptual | interconnect / pipeline |
| Storage vs RAM bandwidth | distinct measurement scope and access paths | RAM + NVMe / pipeline |

Unavailable experiments show requirements while keeping explanation readable.
Accept with novice copy review, correct units, keyboard links and provenance visible.
