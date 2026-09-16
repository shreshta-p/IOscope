# Targeted research — 2026-09-06

Inspected references for architecture and interactions; no code/models imported.
These notes are design inferences from source, not runtime evaluations.

- [ThreeUI](https://github.com/MengTo/threeui) and its
  [package](https://raw.githubusercontent.com/MengTo/threeui/main/package.json):
  community React/WebGL catalog with separated source/assets and licensing notices.
  Adopt isolated interaction primitives, not its catalog architecture. Authored imagery,
  fonts and bundled runtime have distinct notices; review individually before copying.
- [PC Digital Twin source](https://raw.githubusercontent.com/hajaralhadaris/PC-Digital-Twin/main/index.html):
  procedural scene, OrbitControls damping/distance bounds and pointer selection.
  Adopt bounded camera and drag-vs-click interaction requirements; split scene/domain/UI
  responsibilities instead of a monolithic HTML file. No license clearance.
- [PC Anatomy package](https://raw.githubusercontent.com/brickshow/pc-anatomy/main/package.json),
  [README](https://raw.githubusercontent.com/brickshow/pc-anatomy/main/README.md) and
  [models](https://github.com/brickshow/pc-anatomy/tree/main/public/models):
  React/Vite/R3F-oriented application and component models including CPU, GPU, M.2,
  motherboard and RAM; a RAM credits file underscores per-asset provenance.
  Desktop models do not establish laptop physical accuracy. No assets approved.
- [Microsoft DiskSpd parameter reference](https://github.com/microsoft/diskspd/wiki/Command-line-and-parameters):
  outstanding count is per thread/target; one worker/file makes UI cap unambiguous.
  Software caching and write-through are separate controls. Latency/XML collection
  supports normalized final results. Rate limiting is per thread/target.
- [Windows file buffering](https://learn.microsoft.com/en-us/windows/win32/fileio/file-buffering):
  unbuffered access has alignment requirements; query physical sector alignment.
  Cache bypass is not proof of device cache eviction. Separate it from write-through.
- [NVIDIA NVML reference](https://docs.nvidia.com/deploy/nvml-api/nvml-api-reference.html):
  support varies across devices, including non-data-center GPUs. Detect each function
  and preserve unavailable reasons; NVML presence alone is not CUDA pipeline validation.

Research intentionally excludes broad benchmarks and speculative frameworks. Exact
dependency versions and asset revisions are locked only when actually integrated.
