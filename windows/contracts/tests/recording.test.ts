import { readFileSync } from 'node:fs';
import { expect, test } from 'vitest';
import type { ReplaySample, RunMetadata } from '../src/index';
import { validateDomain, validateRecording } from '../src/validate';
const load = <T>(name: string): T =>
  JSON.parse(readFileSync(new URL(`../fixtures/${name}.json`, import.meta.url), 'utf8'));
test('validates a complete fixture recording', () => {
  expect(() => validateRecording(load<RunMetadata>('RunMetadata'), [load<ReplaySample>('ReplaySample')])).not.toThrow();
});
test.each([
  (s: ReplaySample) => {
    s.flow.sequence = 1;
  },
  (s: ReplaySample) => {
    s.flow.paths[0]!.evidence = [];
  },
  (s: ReplaySample) => {
    s.flow.paths[0]!.activity = 0.9;
  },
  (s: ReplaySample) => {
    s.telemetry.measurements[0]!.ageMs = 3001;
  },
  (s: ReplaySample) => {
    s.runId = 'different';
  },
  (s: ReplaySample) => {
    s.analyzerEvents[0]!.runId = 'different';
  },
])('rejects inconsistent replay snapshot %#', (mutate) => {
  const sample = load<ReplaySample>('ReplaySample');
  mutate(sample);
  expect(() => validateDomain('ReplaySample', sample)).toThrow();
});
test.each([
  (r: RunMetadata) => {
    r.endedAt = null;
  },
  (r: RunMetadata) => {
    r.seed = null;
  },
  (r: RunMetadata) => {
    r.outcome = 'aborted';
  },
  (r: RunMetadata) => {
    r.inventory.devices = [];
  },
])('rejects invalid recording metadata %#', (mutate) => {
  const run = load<RunMetadata>('RunMetadata');
  mutate(run);
  expect(() => validateRecording(run, [load<ReplaySample>('ReplaySample')])).toThrow();
});
test('rejects duplicate sequences', () => {
  const sample = load<ReplaySample>('ReplaySample');
  expect(() => validateRecording(load<RunMetadata>('RunMetadata'), [sample, sample])).toThrow();
});
test('rejects a recorded status clock differing from its sample', () => {
  const sample = load<ReplaySample>('ReplaySample');
  sample.workloadStatus.elapsedUs = 42;
  expect(() => validateDomain('ReplaySample', sample)).toThrow();
});
test('rejects forged GPU activity without an explicit stage', () => {
  const sample = load<ReplaySample>('ReplaySample');
  sample.flow.paths.push({ ...sample.flow.paths[0]!, pathId: 'ram-vram-h2d', from: 'ram0', to: 'vram0' });
  expect(() => validateDomain('ReplaySample', sample)).toThrow();
});
test('a completed trace cannot have running metadata', () => {
  const run = load<RunMetadata>('RunMetadata');
  run.outcome = 'running';
  run.endedAt = null;
  expect(() => validateRecording(run, [load<ReplaySample>('ReplaySample')])).toThrow();
});
test('rejects ambiguous analyzer evidence across metric scopes', () => {
  const sample = load<ReplaySample>('ReplaySample');
  sample.telemetry.measurements.push({
    ...sample.telemetry.measurements[0]!,
    metricId: 'cpu.utilization',
    deviceId: 'nvme0',
    unit: 'percent',
    value: 2,
  });
  sample.telemetry.measurements.push({ ...sample.telemetry.measurements.at(-1)!, scope: 'workload', value: 99 });
  sample.analyzerEvents[0]!.evidence = [{ sequence: 0, deviceId: 'nvme0', metricId: 'cpu.utilization' }];
  expect(() => validateRecording(load<RunMetadata>('RunMetadata'), [sample])).toThrow();
});
