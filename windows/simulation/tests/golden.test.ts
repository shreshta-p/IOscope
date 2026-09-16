import { readFileSync } from 'node:fs';
import { expect, test } from 'vitest';
import { validateDomain } from '../../contracts/src/validate';
import { createRecording, generate } from '../src/index';

test('seed 42 matches the reviewed versioned recording byte for byte', () => {
  const text = readFileSync(new URL('../fixtures/seed-42.json', import.meta.url), 'utf8');
  const golden: unknown = JSON.parse(text);
  validateDomain('SimulationRecording', golden);
  expect(JSON.stringify(createRecording(golden.config), null, 2) + '\n').toBe(text);
  expect([...new Set(golden.samples.map((s) => s.phaseId))]).toEqual([
    'prepare',
    'allocate',
    'read',
    'h2d',
    'compute',
    'release',
  ]);
  expect(golden.samples[6]!.flow.paths.find((p) => p.pathId === 'ram-vram-h2d')!.status).toBe('active');
});
test('maximum duration and cadence terminate at the bounded sample count', () => {
  let count = 0;
  for (const frame of generate({
    seed: 4294967295,
    epochUtc: '2026-09-06T00:00:00Z',
    durationSeconds: 600,
    sampleIntervalMs: 100,
    scenario: 'idle',
  })) {
    expect(frame.sequence).toBe(count++);
  }
  expect(count).toBe(6000);
});
