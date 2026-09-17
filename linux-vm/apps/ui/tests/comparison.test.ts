import { expect, test } from 'vitest';
import { createRecording } from '../../../simulation/src/index';
import { compareMetric } from '../src/comparison';
const make = () =>
  createRecording({
    seed: 42,
    epochUtc: '2026-09-06T00:00:00Z',
    durationSeconds: 4,
    sampleIntervalMs: 1000,
    scenario: 'idle',
  });
test('comparison reports counts and absolute deltas', () => {
  const a = make(),
    b = make();
  const result = compareMetric(a, b, 'cpu.utilization');
  expect(result.counts).toEqual([4, 4]);
  expect(result.delta).toBe(0);
  expect(result.percent).toBe(0);
});
test('different windows, origins and scopes cannot be compared', () => {
  const a = make(),
    b = make();
  b.samples[0]!.telemetry.measurements.find((m) => m.metricId === 'cpu.utilization')!.windowMs = 500;
  expect(compareMetric(a, b, 'cpu.utilization').reason).toMatch(/window/);
  b.metadata.origin = 'live';
  expect(compareMetric(a, b, 'cpu.utilization').reason).toMatch(/origin/);
});
test('missing samples and zero baseline never create invented deltas', () => {
  const a = make(),
    b = make();
  for (const s of a.samples) s.telemetry.measurements.find((m) => m.metricId === 'cpu.utilization')!.value = 0;
  expect(compareMetric(a, b, 'cpu.utilization').percent).toBeNull();
  for (const s of b.samples)
    s.telemetry.measurements.find((m) => m.metricId === 'cpu.utilization')!.status = 'unavailable';
  expect(compareMetric(a, b, 'cpu.utilization').counts[1]).toBe(0);
  expect(compareMetric(a, b, 'cpu.utilization').delta).toBeNull();
});
test('percentiles are never averaged', () =>
  expect(compareMetric(make(), make(), 'storage.latency.p95').reason).toMatch(/percentile/));
