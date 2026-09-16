import { test, expect } from 'vitest';
import { liveContinuity } from '../src/live-state';
import type { TelemetryFrame } from '../../../contracts/src/index';
const frame: TelemetryFrame = {
  schemaVersion: '1.0.0',
  origin: 'live',
  sessionId: 'session-a',
  runId: null,
  sequence: 1,
  elapsedUs: 1000000,
  capturedAt: '2026-09-06T00:00:00Z',
  measurements: [],
};
test('live stream rejects simulation, repeats and clock reversal', () => {
  expect(() => liveContinuity(null, { ...frame, origin: 'simulated' })).toThrow(/Synthetic/);
  expect(() => liveContinuity(frame, frame)).toThrow(/order/);
  expect(() => liveContinuity(frame, { ...frame, sequence: 2, elapsedUs: 0 })).toThrow(/order/);
});
test('gaps and new sessions are explicit without interpolating old metrics', () => {
  expect(liveContinuity(frame, { ...frame, sequence: 3 })).toMatch(/gap/);
  expect(liveContinuity(frame, { ...frame, sequence: 0, sessionId: 'new-session' })).toMatch(/restarted/);
  expect(liveContinuity(frame, { ...frame, sequence: 2 })).toBeNull();
});
