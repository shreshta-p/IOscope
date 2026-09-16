import type { HardwareInventory, RunMetadata, TelemetryFrame, WorkloadDefinition } from '../../contracts/src/index';
import type { SimulationConfig } from './config';
import { simulatorVersion } from './generate';

export function createMetadata(config: SimulationConfig, frames: readonly TelemetryFrame[]): RunMetadata {
  const first = frames[0]!,
    last = frames.at(-1)!;
  const devices: HardwareInventory['devices'] = [
    { deviceId: 'cpu0', kind: 'cpu', name: 'Simulated CPU', metrics: [] },
    { deviceId: 'ram0', kind: 'ram', name: 'Simulated 32 GiB RAM', metrics: [] },
    { deviceId: 'nvme0', kind: 'storage', name: 'Simulated NVMe', metrics: [] },
    { deviceId: 'gpu0', kind: 'gpu', name: 'Simulated GPU', metrics: [] },
    { deviceId: 'vram0', kind: 'vram', name: 'Simulated 12 GiB VRAM', metrics: [] },
  ];
  const workload: WorkloadDefinition = {
    schemaVersion: '1.0.0',
    definitionId: config.scenario,
    engine: config.scenario === 'model-loading' ? 'gpu-pipeline' : 'diskspd',
    readPercent: config.scenario === 'mixed' ? 80 : 100,
    blockBytes:
      config.scenario === 'sequential-read' || config.scenario === 'model-loading'
        ? 1048576
        : config.scenario === 'mixed'
          ? 65536
          : 4096,
    queueDepth: config.scenario === 'random-read' ? 8 : config.scenario === 'queue-saturation' ? 32 : 4,
    workingSetBytes: 268435456,
    intensity: 'light',
    pattern: ['random-read', 'mixed', 'queue-saturation'].includes(config.scenario) ? 'random' : 'sequential',
    cacheMode: 'buffered',
    durationSeconds: Math.min(60, config.durationSeconds),
    warmupSeconds: 0,
    cooldownSeconds: 0,
  };
  return {
    schemaVersion: '1.0.0',
    runId: first.runId!,
    origin: 'simulated',
    startedAt: first.capturedAt,
    endedAt: last.capturedAt,
    inventory: { schemaVersion: '1.0.0', inventoryId: first.runId! + '-inventory', platform: 'simulated', devices },
    capabilities: devices.map((device) => ({
      schemaVersion: '1.0.0',
      deviceId: device.deviceId,
      capabilities: first.measurements
        .filter((m) => m.deviceId === device.deviceId)
        .map((m) => ({
          metricId: m.metricId,
          status: m.status,
          requiresElevation: false,
          reason: m.reason,
        })),
    })),
    workload,
    experimentExecutionId: null,
    phaseId: null,
    engine: { name: 'synthetic-profile', version: simulatorVersion, sha256: '0'.repeat(64), argv: [] },
    mappingVersion: '1.0.0',
    analyzerVersion: '1.0.0',
    simulatorVersion,
    seed: config.seed,
    outcome: 'completed',
    abortReason: null,
    uiMode: 'measurement',
    summaries: [],
    artifacts: [],
  };
}
