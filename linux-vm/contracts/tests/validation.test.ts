import { readFileSync, readdirSync } from 'node:fs';
import { describe, expect, test } from 'vitest';
import { validateDomain } from '../src/validate';

const fixture = (name: string): Record<string, unknown> =>
  JSON.parse(readFileSync(new URL(`../fixtures/${name}.json`, import.meta.url), 'utf8'));
describe('domain boundary', () => {
  for (const file of readdirSync(new URL('../fixtures', import.meta.url))) {
    test(`accepts ${file}`, () =>
      expect(() => validateDomain(file.slice(0, -5), fixture(file.slice(0, -5)))).not.toThrow());
  }
  test.each([
    ['queueDepth', 33],
    ['queueDepth', 0],
    ['readPercent', 101],
    ['workingSetBytes', 4294967297],
    ['durationSeconds', 61],
    ['blockBytes', 8192],
    ['command', 'arbitrary'],
  ])('rejects invalid workload %s=%s', (key, value) => {
    expect(() => validateDomain('WorkloadDefinition', { ...fixture('WorkloadDefinition'), [key]: value })).toThrow();
  });
  test.each([{ schemaVersion: '2.0.0' }, { capturedAt: 'not-a-date' }, { origin: 'live' }])(
    'rejects frame header %j',
    (change) => {
      expect(() => validateDomain('TelemetryFrame', { ...fixture('TelemetryFrame'), ...change })).toThrow();
    },
  );
  test.each([
    { status: 'unavailable' },
    { value: null },
    { unit: 'ms' },
    { value: NaN },
    { value: Infinity },
    { value: -1 },
    { metricId: 'invented' },
    { provenance: 'conceptual' },
  ])('rejects dishonest measurement %j', (change) => {
    const frame = fixture('TelemetryFrame');
    const metrics = frame.measurements as Record<string, unknown>[];
    frame.measurements = [{ ...metrics[0], ...change }];
    expect(() => validateDomain('TelemetryFrame', frame)).toThrow();
  });
  test('accepts explicit unavailable instead of zero', () => {
    const frame = fixture('TelemetryFrame');
    const metrics = frame.measurements as Record<string, unknown>[];
    frame.measurements = [{ ...metrics[0], value: null, status: 'unavailable', reason: 'Sensor missing' }];
    expect(() => validateDomain('TelemetryFrame', frame)).not.toThrow();
  });
  test('rejects duplicate metric identity', () => {
    const frame = fixture('TelemetryFrame');
    const metrics = frame.measurements as unknown[];
    metrics.push(metrics[0]);
    expect(() => validateDomain('TelemetryFrame', frame)).toThrow();
  });
  test('recorded origin cannot masquerade as live', () => {
    expect(() => validateDomain('PresentationContext', { ...fixture('PresentationContext'), mode: 'LIVE' })).toThrow();
    expect(() => validateDomain('PresentationContext', { ...fixture('PresentationContext'), runId: null })).toThrow();
  });
  test('rejects unknown contract names', () => expect(() => validateDomain('Unknown', {})).toThrow());
});
