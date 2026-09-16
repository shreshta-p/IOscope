import { expect, test } from 'vitest';
import { validateDomain, validateRecording } from '../../contracts/src/validate';
import { generate } from '../src/generate';
import { deriveFlow } from '../src/flows';
import { createRecording } from '../src/record';
import { scenarios, type SimulationConfig } from '../src/config';
const config: SimulationConfig = {
  seed: 42,
  epochUtc: '2026-09-06T00:00:00Z',
  durationSeconds: 12,
  sampleIntervalMs: 1000,
  scenario: 'model-loading',
};
test('maps known bandwidth endpoints without random decoration', () => {
  const frame = [...generate({ ...config, scenario: 'sequential-read' })][0]!;
  const metric = frame.measurements.find((m) => m.metricId === 'storage.read.bytes_per_second')!;
  metric.value = 1073741824;
  expect(deriveFlow(frame).paths.find((p) => p.pathId === 'nvme-ram-read')!.activity).toBe(1);
  metric.value = 0;
  const path = deriveFlow(frame).paths.find((p) => p.pathId === 'nvme-ram-read')!;
  expect(path.status).toBe('idle');
  expect(path.activity).toBe(0);
  expect(deriveFlow(frame)).toEqual(deriveFlow(structuredClone(frame)));
});
test.each(['stale', 'unavailable'] as const)(
  '%s source suppresses flow without representing measured zero',
  (reason) => {
    const frame = [...generate(config)][0]!;
    const metric = frame.measurements.find((m) => m.metricId === 'storage.read.bytes_per_second')!;
    if (reason === 'stale') metric.ageMs = 3001;
    else {
      metric.status = 'unavailable';
      metric.value = null;
      metric.reason = 'Missing';
    }
    const path = deriveFlow(frame).paths.find((p) => p.pathId === 'nvme-ram-read')!;
    expect(path.status).toBe('unknown');
    expect(path.activity).toBe(0);
    expect(path.evidence).toEqual([]);
  },
);
test('queue average remains fractional and separate from configured cap', () => {
  const frame = [...generate({ ...config, scenario: 'mixed' })][0]!;
  const queue = deriveFlow(frame, 8).queue;
  expect(queue.observedAverage).toBe(3.3);
  expect(queue.configuredLimit).toBe(8);
  expect(queue.representation).toBe('aggregated');
});
test('GPU paths activate only during explicitly marked stages', () => {
  for (const frame of generate(config)) {
    const stage = frame.measurements.find((m) => m.metricId === 'pipeline.stage')!.value;
    const paths = deriveFlow(frame).paths;
    expect(paths.find((p) => p.pathId === 'ram-vram-h2d')!.status === 'active').toBe(stage === 3);
    expect(paths.find((p) => p.pathId === 'vram-gpu-compute')!.status === 'active').toBe(stage === 4);
  }
  const frame = [...generate({ ...config, scenario: 'sequential-read' })][0]!;
  expect(
    deriveFlow(frame)
      .paths.filter((p) => p.pathId.includes('vram'))
      .every((p) => p.status === 'unknown'),
  ).toBe(true);
});
test.each(scenarios)('%s records self-contained valid replay samples', (scenario) => {
  const recording = createRecording({ ...config, scenario });
  expect(() => validateRecording(recording.metadata, recording.samples)).not.toThrow();
  expect(recording.samples).toHaveLength(12);
  expect(recording.metadata.origin).toBe('simulated');
  for (const sample of recording.samples) expect(() => validateDomain('ReplaySample', sample)).not.toThrow();
  const copied = JSON.parse(JSON.stringify(recording));
  expect(copied.samples[7]).toEqual(recording.samples[7]);
  expect(JSON.stringify(recording)).toBe(JSON.stringify(createRecording({ ...config, scenario })));
});
test('missing pipeline stage cannot invent transfer evidence or phase', () => {
  const recording = createRecording({ ...config, unavailableMetrics: ['pipeline.stage'] });
  expect(recording.samples.every((s) => s.phaseId === null)).toBe(true);
  expect(
    recording.samples.every((s) =>
      s.flow.paths.filter((p) => p.pathId.includes('vram')).every((p) => p.status === 'unknown'),
    ),
  ).toBe(true);
});
