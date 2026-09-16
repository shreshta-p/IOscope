import type { Measurement, TelemetryFrame } from '../../contracts/src/index';
import { metricCatalog } from '../../contracts/src/metrics';
import { validateDomain } from '../../contracts/src/validate';
import { normalizeConfig, type SimulationConfig } from './config';
import { createRandom } from './prng';
import { GiB, profile } from './profiles';

export const simulatorVersion = '1.0.0';
export function simulationId(config: SimulationConfig): string {
  // Stable non-cryptographic identity over canonical input; not an integrity digest.
  let hash = 2166136261;
  for (const char of JSON.stringify(config)) hash = Math.imul(hash ^ char.charCodeAt(0), 16777619) >>> 0;
  return `sim-${config.scenario}-${config.seed}-${hash.toString(16)}`;
}
export function generate(input: SimulationConfig): Generator<TelemetryFrame, void, unknown> {
  const config = normalizeConfig(input);
  return frames(config);
}
function* frames(config: SimulationConfig): Generator<TelemetryFrame, void, unknown> {
  const random = createRandom(config.seed);
  const count = Math.ceil((config.durationSeconds * 1000) / config.sampleIntervalMs);
  const runId = simulationId(config);
  const missing = new Set(config.unavailableMetrics);
  const epoch = Date.parse(config.epochUtc);
  for (let sequence = 0; sequence < count; sequence++) {
    const p = profile(config.scenario, count === 1 ? 0 : sequence / (count - 1), random());
    const measurements: Measurement[] = [];
    const add = (metricId: string, deviceId: string, value: number | null, scope: Measurement['scope'] = 'system') => {
      const definition = metricCatalog.get(metricId)!;
      const unavailable = missing.has(metricId) || value === null;
      measurements.push({
        metricId,
        deviceId,
        scope,
        unit: definition.unit as Measurement['unit'],
        value: unavailable ? null : Math.round(value! * 1000000) / 1000000,
        status: unavailable ? 'unavailable' : 'available',
        provenance: 'simulated',
        source: 'simulation.v1',
        ageMs: 0,
        windowMs: config.sampleIntervalMs,
        reason: unavailable
          ? missing.has(metricId)
            ? 'Injected unavailable synthetic sensor'
            : 'Not modeled in this scenario'
          : null,
      });
    };
    add('cpu.utilization', 'cpu0', p.cpu);
    add('cpu.temperature', 'cpu0', 50);
    add('ram.total', 'ram0', 32 * GiB);
    add('ram.available', 'ram0', p.ramAvailable);
    add('storage.capacity', 'nvme0', 2 * 1024 * GiB);
    add('storage.read.bytes_per_second', 'nvme0', p.read);
    add('storage.write.bytes_per_second', 'nvme0', p.write);
    add('storage.iops', 'nvme0', (p.read + p.write) / p.blockBytes);
    add('storage.queue.average', 'nvme0', p.queue);
    add('storage.latency.mean', 'nvme0', p.latency);
    add('storage.latency.p95', 'nvme0', null, 'workload');
    add('storage.temperature', 'nvme0', 40);
    add('gpu.utilization', 'gpu0', p.gpu);
    add('gpu.temperature', 'gpu0', p.temperature);
    add('gpu.clock', 'gpu0', p.gpu > 10 ? 1500 : 300);
    add('gpu.power', 'gpu0', 15 + p.gpu);
    add('vram.total', 'vram0', 12 * GiB);
    add('vram.used', 'vram0', p.vramUsed);
    add('pipeline.stage', 'gpu0', p.stage, 'workload');
    add('pipeline.h2d.bytes_per_second', 'gpu0', p.stage === null ? null : p.h2d, 'workload');
    const frame: TelemetryFrame = {
      schemaVersion: '1.0.0',
      sessionId: runId + '-session',
      runId,
      origin: 'simulated',
      sequence,
      elapsedUs: sequence * config.sampleIntervalMs * 1000,
      capturedAt: new Date(epoch + sequence * config.sampleIntervalMs).toISOString(),
      measurements,
    };
    validateDomain('TelemetryFrame', frame);
    yield frame;
  }
}
