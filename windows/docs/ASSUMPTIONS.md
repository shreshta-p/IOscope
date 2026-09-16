# Assumptions and feasibility register

| Item | Decision / consequence | Revisit |
|---|---|---|
| Machine specs supplied | target profile, not hardware discovery evidence | Phase 5 |
| Empty repository | greenfield; no existing APIs or assets to preserve | established Phase 0 |
| C++ tooling | VS2019 Build Tools/MSVC 14.29 found; cl/cmake absent from current PATH | native build preflight |
| Target compiler | VS2022 Build Tools v143 + Windows SDK recommended for new C++20 work | before native build |
| GPU sensors | NVML capability-by-function; laptop counters may be missing | Phase 5 |
| CUDA | toolkit/runtime and working compiler not verified | Phase 7E prerequisite |
| SSD temperature | may be unavailable without extra integration; restricted workload policy applies | Phase 5/6 |
| Queue visualization | PDH aggregate + configured cap, no exact hardware requests | only add tracing with measured need |
| Latency | live average possible; workload p95 after DiskSpd result | Phase 6 |
| Cold cache | cannot guarantee safe eviction; first/repeated with preparation disclosed | Phase 7D |
| Laptop geometry | illustrative fixed topology, not OEM CAD | asset design |
| Reference assets | no licenses cleared for import; purpose-built geometry | Phase 3 |
| Run path | repo is in OneDrive; runtime DB/scratch must be outside it | packaging |
| DiskSpd live updates | not a streaming domain API; phase restart for parameter changes | Phase 6 |
| License | user has not selected project distribution license; no release now | before public distribution |

No user decision blocks Phase 1. Native build prerequisites are a future setup issue;
do not install toolchains or run storage/GPU activity as part of this baseline.
