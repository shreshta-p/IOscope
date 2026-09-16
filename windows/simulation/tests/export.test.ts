import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { expect, test } from 'vitest';
import { validateDomain } from '../../contracts/src/validate';
test('CLI exports reproducible files in separate processes and refuses overwrite', () => {
  const directory = mkdtempSync(join(tmpdir(), 'ioscope-export-'));
  try {
    const run = (name: string, seed = '42') =>
      spawnSync(
        process.execPath,
        [
          '--import',
          'tsx',
          resolve('tools/export-simulation.ts'),
          '--scenario',
          'model-loading',
          '--seed',
          seed,
          '--duration',
          '12',
          '--output',
          join(directory, name),
        ],
        { encoding: 'utf8' },
      );
    const first = run('one.json');
    expect(first.status, first.stderr).toBe(0);
    const second = run('two.json');
    expect(second.status, second.stderr).toBe(0);
    const original = readFileSync(join(directory, 'one.json'), 'utf8');
    expect(original).toBe(readFileSync(join(directory, 'two.json'), 'utf8'));
    expect(() => validateDomain('SimulationRecording', JSON.parse(original))).not.toThrow();
    expect(run('one.json').status).not.toBe(0);
    expect(readFileSync(join(directory, 'one.json'), 'utf8')).toBe(original);
    expect(run('invalid.json', '-1').status).not.toBe(0);
  } finally {
    rmSync(directory, { recursive: true });
  }
}, 15000);
