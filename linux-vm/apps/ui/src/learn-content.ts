// Phase 9 (docs/11-LEARN-SPEC.md): 13 short topics, each with exactly four
// sections (what/why/see in system/try yourself) -- no textbook-sized pages.
// componentId drives the same selection state the digital twin uses; a null
// experimentId means no real, working experiment maps to this topic honestly
// (rather than linking somewhere misleading).
export interface LearnTopic {
  id: string;
  title: string;
  what: string;
  why: string;
  seeInSystem: string;
  tryYourself: string;
  componentId: 'cpu0' | 'ram0' | 'nvme0' | 'gpu0' | 'vram0';
  experimentId: string | null;
}

export const learnTopics: LearnTopic[] = [
  {
    id: 'queue-depth',
    title: 'Queue depth',
    what: 'The number of storage requests allowed to be outstanding (issued but not yet completed) at once.',
    why: 'A configured outstanding-request limit is a setting, not a measurement — the system queue actually observed can differ from it.',
    seeInSystem:
      'The NVMe queue inspector on the Live page shows both the configured limit and the observed average side by side.',
    tryYourself: 'Run the queue depth sweep to see throughput and latency respond as this limit changes.',
    componentId: 'nvme0',
    experimentId: 'qd-sweep',
  },
  {
    id: 'iops',
    title: 'IOPS',
    what: 'Input/output operations completed per second — a count of operations, not bytes.',
    why: 'Two workloads can move the same bytes per second with very different IOPS if their block sizes differ.',
    seeInSystem: 'The Workload Lab and Experiments results tables report storage.iops for every real run.',
    tryYourself: 'Run the block-size sweep: IOPS falls as block size grows even though throughput stays similar.',
    componentId: 'nvme0',
    experimentId: 'block-sweep',
  },
  {
    id: 'throughput',
    title: 'Throughput',
    what: 'Bytes transferred per second, over a stated measurement scope.',
    why: 'Block size times operations only equals throughput when both numbers share the same scope and time window — they are not always comparable across runs.',
    seeInSystem:
      'The Live page storage strip and every run’s summary table report storage.read.bytes_per_second directly, never estimated.',
    tryYourself: 'Run the block-size sweep and compare throughput to IOPS side by side.',
    componentId: 'nvme0',
    experimentId: 'block-sweep',
  },
  {
    id: 'latency',
    title: 'Latency',
    what: 'The elapsed time for one storage operation to complete, from issue to completion.',
    why: 'A mean latency can look fine while a tail (p95) latency is much worse — averages hide that difference.',
    seeInSystem: 'Every real run records both storage.latency.mean and storage.latency.p95, never just one.',
    tryYourself:
      'Run the queue depth sweep: latency climbs sharply at higher queue depths even as throughput plateaus.',
    componentId: 'nvme0',
    experimentId: 'qd-sweep',
  },
  {
    id: 'block-size',
    title: 'Block size',
    what: 'The number of bytes read or written per individual storage operation.',
    why: 'Equal byte volume is not equal operations: a 1 MiB block and a 4 KiB block moving the same total data produce very different IOPS.',
    seeInSystem: 'The Workload Lab lets you choose a block size directly and see its effect on the admitted workload.',
    tryYourself: 'Run the block-size sweep to see the relationship directly, from 4 KiB to 1 MiB.',
    componentId: 'nvme0',
    experimentId: 'block-sweep',
  },
  {
    id: 'sequential-random',
    title: 'Sequential vs random',
    what: 'Sequential access reads or writes contiguous bytes; random access jumps between unrelated locations.',
    why: 'Access order is a description of the pattern, not an inherent guarantee of speed — the actual effect depends on the device and cache state.',
    seeInSystem:
      'The Workload Lab and every experiment definition record which access pattern was actually used for a run.',
    tryYourself:
      'No dedicated sequential-vs-random experiment exists yet; compare the pattern field across saved runs on the Runs page instead.',
    componentId: 'nvme0',
    experimentId: null,
  },
  {
    id: 'buffered-io',
    title: 'Buffered I/O',
    what: 'Buffered I/O lets the OS page cache participate in a read or write; unbuffered (direct) I/O bypasses it.',
    why: 'The distinction is about which cache layer participates, not automatically about which mode is faster.',
    seeInSystem:
      'The Workload Lab’s cache mode control and every recording’s workload.cacheMode field show which mode actually ran.',
    tryYourself: 'Run the buffered-vs-unbuffered experiment and compare the two modes directly, in both orders.',
    componentId: 'ram0',
    experimentId: 'cache-mode',
  },
  {
    id: 'page-cache',
    title: 'Page cache',
    what: 'A conceptual, RAM-backed cache of recently accessed file data, managed by the OS.',
    why: 'Cache hits are not directly observed by this project — only their effect on measured latency and throughput can be seen, never the cache itself.',
    seeInSystem:
      'The Live page’s RAM available metric shows overall memory pressure, which page cache usage contributes to.',
    tryYourself:
      'Run the first/repeated access experiment: a repeated read may be faster, but the first pass is never claimed to be genuinely cold.',
    componentId: 'ram0',
    experimentId: 'access-pass',
  },
  {
    id: 'working-set',
    title: 'Working set',
    what: 'The size of the dataset a workload repeatedly reads or writes during a run.',
    why: 'This is the generated test dataset’s size, a workload control — not the same thing as an OS process’s memory working set.',
    seeInSystem: 'The Workload Lab’s trial footprint panel shows the exact working set size before a run starts.',
    tryYourself:
      'Run the first/repeated access experiment, which fixes the working set and varies only the access pass.',
    componentId: 'ram0',
    experimentId: 'access-pass',
  },
  {
    id: 'gpu-vram',
    title: 'GPU VRAM',
    what: 'Dedicated memory on a GPU device, separate from system RAM.',
    why: 'VRAM allocation describes device memory usage, not GPU compute activity — a device can hold data without actively computing on it.',
    seeInSystem:
      'The Live page’s VRAM inspector reports used/total VRAM when a supported GPU is present, and honestly "Unavailable" otherwise.',
    tryYourself:
      'The storage-to-GPU pipeline experiment covers this, but requires a supported GPU (not available on this system).',
    componentId: 'vram0',
    experimentId: 'gpu-pipeline',
  },
  {
    id: 'host-to-device',
    title: 'Host-to-device transfer',
    what: 'An explicit copy of data from system RAM to GPU device memory, over the system’s interconnect.',
    why: 'This project stages the transfer explicitly rather than claiming DirectStorage- or GPUDirect-style zero-copy behavior.',
    seeInSystem:
      'The pipeline stage marker and pipeline.h2d.duration metric, when a supported GPU is present, record this stage separately from the storage read.',
    tryYourself:
      'The storage-to-GPU pipeline experiment covers this, but requires a supported GPU (not available on this system).',
    componentId: 'vram0',
    experimentId: 'gpu-pipeline',
  },
  {
    id: 'pcie',
    title: 'PCIe',
    what: 'The shared interconnect that storage, GPU and other devices use to communicate with the rest of the system.',
    why: 'The interconnect is shared infrastructure; its path animation in the Live view is conceptual, not a per-lane measurement.',
    seeInSystem:
      'The Live page’s data-path view shows an aggregated, conceptual representation of activity across the interconnect.',
    tryYourself:
      'The storage-to-GPU pipeline experiment covers this, but requires a supported GPU (not available on this system).',
    componentId: 'gpu0',
    experimentId: 'gpu-pipeline',
  },
  {
    id: 'storage-vs-ram-bandwidth',
    title: 'Storage vs RAM bandwidth',
    what: 'Storage and RAM are measured over distinct scopes and access paths, and their bandwidth numbers are not directly interchangeable.',
    why: 'Comparing them requires knowing which path and which measurement window each number came from.',
    seeInSystem:
      'The Live page’s system strip shows both RAM available and storage read throughput side by side, each with its own measurement scope.',
    tryYourself:
      'The storage-to-GPU pipeline experiment covers this, but requires a supported GPU (not available on this system).',
    componentId: 'nvme0',
    experimentId: 'gpu-pipeline',
  },
];
