import { expect, test } from 'vitest';
import { validateDomain } from '../../contracts/src/validate';
import { generate } from '../src/generate';
import { scenarios, type SimulationConfig } from '../src/config';
const base: SimulationConfig = {
  seed: 42,
  epochUtc: '2026-09-06T00:00:00Z',
  durationSeconds: 10,
  sampleIntervalMs: 1000,
  scenario: 'mixed',
};
test('seeded output survives separate generation and serialization', () => {
  const first = [...generate(base)];
  expect(first).toHaveLength(10);
  expect(JSON.stringify(first)).toBe(JSON.stringify([...generate({ ...base })]));
  expect(JSON.stringify(first)).not.toBe(JSON.stringify([...generate({ ...base, seed: 43 })]));
});
test('clock uses explicit epoch and half-open sampling interval', () => {
  const frames = [...generate({ ...base, durationSeconds: 1, sampleIntervalMs: 300 })];
  expect(frames.map((f) => f.elapsedUs)).toEqual([0, 300000, 600000, 900000]);
  expect(frames.map((f) => f.sequence)).toEqual([0, 1, 2, 3]);
  expect(frames[3]!.capturedAt).toBe('2026-09-06T00:00:00.900Z');
});
test.each(scenarios)('%s emits only bounded synthetic telemetry', (scenario) => {
  const frames = [...generate({ ...base, scenario })];
  for (const frame of frames) {
    expect(() => validateDomain('TelemetryFrame', frame)).not.toThrow();
    expect(frame.origin).toBe('simulated');
    expect(frame.measurements.every((m) => m.provenance === 'simulated')).toBe(true);
  }
});
test.each([
  { seed: -1 },
  { seed: 4294967296 },
  { seed: 0.5 },
  { seed: NaN },
  { durationSeconds: 0 },
  { durationSeconds: 601 },
  { durationSeconds: 1.2 },
  { sampleIntervalMs: 99 },
  { sampleIntervalMs: 1001 },
  { sampleIntervalMs: 100.5 },
  { epochUtc: 'not-a-date' },
  { epochUtc: '2026-02-30T00:00:00Z' },
  { epochUtc: '2026-09-06' },
  { scenario: 'invented' },
  { unavailableMetrics: ['invented'] },
])('rejects invalid configuration %j', (change) => {
  expect(() => [...generate({ ...base, ...change } as SimulationConfig)]).toThrow();
});
test('zero seed is deterministic and does not collapse to constant output', () => {
  const frames = [...generate({ ...base, seed: 0 })];
  expect(frames).toEqual([...generate({ ...base, seed: 0 })]);
  const cpu = frames.map((f) => f.measurements.find((m) => m.metricId === 'cpu.utilization')!.value);
  expect(new Set(cpu).size).toBeGreaterThan(1);
});
test('unavailable injection keeps null with a reason on every tick', () => {
  for (const frame of generate({ ...base, unavailableMetrics: ['storage.read.bytes_per_second'] })) {
    const metric = frame.measurements.find((m) => m.metricId === 'storage.read.bytes_per_second')!;
    expect(metric.value).toBeNull();
    expect(metric.status).toBe('unavailable');
    expect(metric.reason).toBeTruthy();
  }
});
test('queue pressure and memory pressure profiles communicate their stated behavior', () => {
  const value = (config: SimulationConfig, metricId: string) =>
    [...generate(config)].map((f) => f.measurements.find((m) => m.metricId === metricId)!.value!);
  const queue = value({ ...base, scenario: 'queue-saturation' }, 'storage.queue.average');
  const latency = value({ ...base, scenario: 'queue-saturation' }, 'storage.latency.mean');
  expect(queue.at(-1)).toBeGreaterThan(queue[0]! + 10);
  expect(latency.at(-1)).toBeGreaterThan(latency[0]! * 2);
  const ram = value({ ...base, scenario: 'memory-pressure' }, 'ram.available');
  expect(ram.at(-1)).toBeLessThan(ram[0]! / 4);
  const temperature = value({ ...base, scenario: 'thermal-warning' }, 'gpu.temperature');
  expect(temperature.at(-1)).toBeGreaterThan(80);
  expect(temperature[0]).toBeLessThan(80);
});
test('model loading publishes explicit stage markers', () => {
  const frames = [...generate({ ...base, scenario: 'model-loading', durationSeconds: 12 })];
  const stages = frames.map((f) => f.measurements.find((m) => m.metricId === 'pipeline.stage')?.value);
  expect([...new Set(stages)]).toEqual([0, 1, 2, 3, 4, 5]);
});
test('consumer mutations do not alter future samples or caller configuration', () => {
  const config = { ...base, unavailableMetrics: ['gpu.temperature'] };
  const iterator = generate(config)[Symbol.iterator]();
  const first = iterator.next().value!;
  first.measurements[0]!.value = -999;
  config.unavailableMetrics.push('cpu.utilization');
  const next = iterator.next().value!;
  expect(next.measurements[0]!.value).not.toBe(-999);
  expect(next.measurements.find((m) => m.metricId === 'cpu.utilization')!.status).toBe('available');
});
