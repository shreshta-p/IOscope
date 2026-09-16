import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { parseArgs } from 'node:util';
import { createRecording } from '../simulation/src/index';
import { validateDomain } from '../contracts/src/validate';

try {
  const { values } = parseArgs({
    options: {
      scenario: { type: 'string', default: 'model-loading' },
      seed: { type: 'string', default: '42' },
      duration: { type: 'string', default: '12' },
      interval: { type: 'string', default: '1000' },
      epoch: { type: 'string', default: '2026-09-06T00:00:00Z' },
      output: { type: 'string', default: 'out/simulation.json' },
    },
    strict: true,
    allowPositionals: false,
  });
  const config = {
    seed: Number(values.seed),
    epochUtc: values.epoch,
    durationSeconds: Number(values.duration),
    sampleIntervalMs: Number(values.interval),
    scenario: values.scenario,
  };
  validateDomain('SimulationConfig', config);
  const recording = createRecording(config);
  const output = resolve(values.output);
  await mkdir(dirname(output), { recursive: true });
  await writeFile(output, JSON.stringify(recording, null, 2) + '\n', { encoding: 'utf8', flag: 'wx' });
  console.log(
    JSON.stringify({
      level: 'info',
      event: 'simulation.exported',
      origin: 'simulated',
      samples: recording.samples.length,
      output,
    }),
  );
} catch (error) {
  console.error(
    JSON.stringify({
      level: 'error',
      event: 'simulation.export.failed',
      message: error instanceof Error ? error.message : 'Unknown export error',
    }),
  );
  process.exitCode = 1;
}
