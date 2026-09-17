import type { ExperimentDefinition, WorkloadDefinition } from '../../../contracts/src/index';

const base = (overrides: Partial<WorkloadDefinition>): WorkloadDefinition => ({
  schemaVersion: '1.0.0',
  definitionId: 'phase',
  engine: 'diskspd',
  readPercent: 100,
  blockBytes: 4096,
  queueDepth: 1,
  workingSetBytes: 64 * 1024 ** 2,
  intensity: 'light',
  pattern: 'random',
  cacheMode: 'buffered',
  durationSeconds: 5,
  warmupSeconds: 0,
  cooldownSeconds: 0,
  ...overrides,
});

const queueDepthSweep: ExperimentDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'qd-sweep',
  title: 'Queue depth sweep',
  question: 'How does concurrency affect storage throughput and latency?',
  concept: 'A configured outstanding-request limit is distinct from the observed system queue.',
  variable: 'queueDepth',
  hypothesis: 'Additional concurrency may improve throughput, then only add latency.',
  phases: [1, 2, 4, 8, 16, 32].map((queueDepth, ordinal) => ({
    schemaVersion: '1.0.0',
    phaseId: `qd${queueDepth}`,
    ordinal,
    purpose: `Observe queue depth ${queueDepth}`,
    workload: base({ definitionId: `qd-phase-${ordinal}`, queueDepth, durationSeconds: 5 }),
    observe: ['storage.read.bytes_per_second', 'storage.iops', 'storage.latency.mean'],
    settleSeconds: 2,
  })) as ExperimentDefinition['phases'],
};

const blockSizeSweep: ExperimentDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'block-sweep',
  title: 'Block size sweep',
  question: 'How does block size affect throughput and operations per second?',
  concept: 'Equal byte volume is not equal operations: larger blocks mean fewer, bigger requests.',
  variable: 'blockBytes',
  hypothesis: 'Throughput may stay similar across block sizes while IOPS falls as blocks grow.',
  phases: ([4096, 16384, 65536, 262144, 1048576] as const).map((blockBytes, ordinal) => ({
    schemaVersion: '1.0.0',
    phaseId: `bs${blockBytes / 1024}k`,
    ordinal,
    purpose: `Observe ${blockBytes / 1024} KiB blocks`,
    workload: base({
      definitionId: `bs-phase-${ordinal}`,
      blockBytes,
      queueDepth: 4,
      pattern: 'sequential',
      workingSetBytes: 1024 * 1024 ** 2,
      durationSeconds: 15,
    }),
    observe: ['storage.read.bytes_per_second', 'storage.iops'],
    settleSeconds: 2,
  })) as ExperimentDefinition['phases'],
};

const cacheModeExperiment: ExperimentDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'cache-mode',
  title: 'Buffered vs unbuffered',
  question: 'Does bypassing the OS page cache change read behavior?',
  concept: 'Unbuffered reads bypass the OS page cache; order should not matter if it does.',
  variable: 'cacheMode',
  hypothesis: 'Unbuffered reads may behave differently from buffered reads, independent of order.',
  phases: (['buffered', 'unbuffered', 'unbuffered', 'buffered'] as const).map((cacheMode, ordinal) => ({
    schemaVersion: '1.0.0',
    phaseId: `cache${ordinal}-${cacheMode}`,
    ordinal,
    purpose: `Observe ${cacheMode} reads (pass ${ordinal})`,
    workload: base({
      definitionId: `cache-phase-${ordinal}`,
      cacheMode,
      queueDepth: 4,
      blockBytes: 65536,
      workingSetBytes: 512 * 1024 ** 2,
      durationSeconds: 10,
    }),
    observe: ['storage.read.bytes_per_second', 'storage.latency.mean'],
    settleSeconds: 2,
  })) as ExperimentDefinition['phases'],
};

const accessPassExperiment: ExperimentDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'access-pass',
  title: 'First vs repeated access',
  question: 'Does a repeated read differ from the first, over the same dataset?',
  concept: 'Preparation may already warm the page cache; never claim a genuinely cold first pass.',
  variable: 'accessPass',
  hypothesis: 'A repeated pass may show lower latency, but cache state is not fully controlled.',
  phases: (['first', 'repeated'] as const).map((pass, ordinal) => ({
    schemaVersion: '1.0.0',
    phaseId: `${pass}-pass`,
    ordinal,
    purpose: `Observe the ${pass} pass over the same prepared dataset`,
    workload: base({
      definitionId: `access-${pass}`,
      blockBytes: 65536,
      queueDepth: 4,
      pattern: 'sequential',
      workingSetBytes: 64 * 1024 ** 2,
      durationSeconds: 5,
    }),
    observe: ['storage.read.bytes_per_second', 'storage.latency.mean'],
    settleSeconds: 2,
  })) as ExperimentDefinition['phases'],
};

// 7E (storage-to-GPU pipeline) is capability-gated only, per docs/07-EXPERIMENT-SPEC.md
// and the agent's gpu_capability.hpp: no execution engine exists on this platform. It
// is listed so the catalog is honest about what exists, not silently omitted, but
// selecting it always shows the capability-probe denial rather than a working profile.
const gpuPipelineExperiment: ExperimentDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'gpu-pipeline',
  title: 'Storage-to-GPU pipeline',
  question: 'Does staged host-to-device transfer add measurable overhead?',
  concept: 'Explicit staged host read plus device copy, not DirectStorage or GPUDirect.',
  variable: 'pipelineStage',
  hypothesis: 'Host-to-device transfer adds a measurable, separately timed phase.',
  phases: [
    {
      schemaVersion: '1.0.0',
      phaseId: 'stage0',
      ordinal: 0,
      purpose: 'Observe staged host-to-device transfer',
      workload: base({ definitionId: 'gpu-stage0', engine: 'gpu-pipeline' }),
      observe: ['storage.read.bytes_per_second'],
      settleSeconds: 0,
    },
  ] as ExperimentDefinition['phases'],
};

export const experimentCatalog: ExperimentDefinition[] = [
  queueDepthSweep,
  blockSizeSweep,
  cacheModeExperiment,
  accessPassExperiment,
  gpuPipelineExperiment,
];
