# Experiments

Each definition includes question, concept, controlled variable, constants, observations,
general hypothesis, ordered phases and workload definitions. Each execution records
preparation, measurement, settling and outcome. Stop cancels current and future phases.
One experiment at a time; validate aggregate disk/write/time budget before phase one.
Each workload phase has a separate run linked by experiment execution ID.

| Experiment | Variable / sequence | Constant controls | Observe / interpretation |
|---|---|---|---|
| Queue depth | 1,2,4,8,16,32 | 4KiB random read, 512MiB, moderate, 15s | workload IOPS/mean/p95 and system queue; concurrency may improve throughput then latency |
| Block size | 4,16,64,256,1024 KiB | sequential read, QD4, 1GiB, moderate, 15s | throughput vs operations; equal byte volume is not equal operations |
| Buffered/unbuffered | buffered then unbuffered; repeat reversed | read-only, QD4, 64KiB, 512MiB | OS cache bypass effect; write-through unchanged |
| First/repeated access | first pass then repeated pass | same prepared dataset, buffered sequential read | preparation may warm cache; never claim genuinely cold |
| Storage-to-GPU | allocate, read, H2D, optional compute, release | 256MiB default; exact buffer types recorded | host and CUDA event times in separate clock domains |

Settling has no benchmark samples in statistics. Default 2s between phases, maximum
experiment wall time 600s including preparation/cooldown. If quotas cannot cover the
whole plan, reject before allocating. Run each experiment serially in implementation order.

A safe cold cache cannot be guaranteed by file creation or FlushFileBuffers. No global
cache purge. Label “First-access vs repeated-access behavior (cache state uncontrolled)”.
Record access order and initialization; results may reflect cache, temperature, other
processes and the chosen rate cap. Never guarantee a faster warm pass.

GPU pipeline is an optional capability-gated native helper. Use staged host read plus
explicit H2D, no DirectStorage/GPUDirect claim. CUDA runtime/device presence must be
probed; NVML alone does not establish CUDA availability. Default pageable memory;
pinned memory is a separately recorded future controlled variable, not a silent optimization.
Host allocation/read use monotonic host time; CUDA transfer/compute use CUDA events and
explicit synchronization. Total wall time includes all phases. Small checksum kernel,
if implemented, validates transferred data rather than claiming model inference.
Unsupported hardware disables only this experiment; simulation remains labeled.

Results show sample counts, outcome and controls before interpretation. Save/replay
restores phase markers and original evidence. Compare compatible phases only.
