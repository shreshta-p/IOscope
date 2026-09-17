import { test, expect } from 'vitest';
import { createRecording } from '../../../simulation/src/index';
import { sampleAt, changeSource, type Session } from '../src/session';
const recording = createRecording({
  seed: 42,
  epochUtc: '2026-09-06T00:00:00Z',
  durationSeconds: 4,
  sampleIntervalMs: 1000,
  scenario: 'idle',
});
test('seek selects preceding sample and clamps endpoints', () => {
  expect(sampleAt(recording, 1500)).toBe(1);
  expect(sampleAt(recording, -10)).toBe(0);
  expect(sampleAt(recording, 99999)).toBe(3);
});
test('live does not silently substitute simulation', () => {
  const session: Session = { mode: 'SIMULATED', recording, index: 0, playing: true, speed: 1, live: null };
  const next = changeSource(session, 'LIVE');
  expect(next.recording).toBeNull();
  expect(next.live).toBeNull();
  expect(next.mode).toBe('LIVE');
});
test('replay preserves synthetic origin and starts paused', () => {
  const session: Session = { mode: 'LIVE', recording: null, index: 0, playing: false, speed: 1, live: null };
  const next = changeSource(session, 'RECORDED', recording);
  expect(next.recording?.metadata.origin).toBe('simulated');
  expect(next.playing).toBe(false);
});
