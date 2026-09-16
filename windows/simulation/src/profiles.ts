import type { Scenario } from './config';
export const MiB = 1048576;
export const GiB = 1024 * MiB;
/** Authored educational profiles, not fitted hardware models. */
export function profile(scenario: Scenario, progress: number, noise: number) {
  let read = 0,
    write = 0,
    queue = 0,
    latency = 0,
    cpu = 2 + noise;
  let ramAvailable = 24 * GiB,
    vramUsed = GiB,
    gpu = 1;
  let temperature = 45,
    stage: number | null = null,
    h2d = 0;
  let blockBytes = 4096;
  const jitter = 0.96 + noise * 0.08;
  switch (scenario) {
    case 'idle':
      read = MiB * noise;
      break;
    case 'sequential-read':
      read = 800 * MiB * jitter;
      queue = 3.5;
      latency = 0.7;
      cpu = 8 + noise;
      blockBytes = MiB;
      break;
    case 'random-read':
      read = 180 * MiB * jitter;
      queue = 7.5;
      latency = 0.18;
      cpu = 14 + noise;
      break;
    case 'mixed':
      read = 120 * MiB * jitter;
      write = 30 * MiB * jitter;
      queue = 3.3;
      latency = 0.9;
      cpu = 12 + noise;
      blockBytes = 65536;
      break;
    case 'queue-saturation':
      read = (80 + 160 * Math.min(progress * 3, 1)) * MiB * jitter;
      queue = 1 + 30 * progress;
      latency = 0.1 + 3 * progress * progress;
      cpu = 10 + noise;
      break;
    case 'memory-pressure':
      ramAvailable = (24 - 23 * progress) * GiB;
      cpu = 30 + noise;
      read = 25 * MiB;
      write = 15 * MiB;
      queue = 2;
      latency = 2;
      break;
    case 'thermal-warning':
      temperature = 65 + 18 * progress;
      gpu = 65;
      cpu = 20 + noise;
      break;
    case 'model-loading':
      stage = Math.min(5, Math.floor(progress * 6));
      ramAvailable = (stage >= 1 && stage < 5 ? 23.75 : 24) * GiB;
      vramUsed = (stage >= 3 && stage < 5 ? 1.25 : 1) * GiB;
      read = stage === 2 ? 128 * MiB * jitter : 0;
      h2d = stage === 3 ? 256 * MiB * jitter : 0;
      gpu = stage === 4 ? 60 + noise : 1;
      blockBytes = MiB;
      queue = stage === 2 ? 1 : 0;
      latency = stage === 2 ? 1 : 0;
      break;
  }
  return { read, write, queue, latency, cpu, ramAvailable, vramUsed, gpu, temperature, stage, h2d, blockBytes };
}
