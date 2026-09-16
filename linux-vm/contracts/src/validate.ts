import Ajv2020 from 'ajv/dist/2020.js';
import addFormats from 'ajv-formats';
import schema from '../v1/domain.schema.json';
import type { DomainTypes, TelemetryFrame, ReplaySample, RunMetadata, WorkloadStatus } from './index';
import { invariant, metricCatalog, validateMetrics } from './metrics';
import { resolveEvidence, validateFlow } from './flow-mapping';

const ajv = new Ajv2020({ allErrors: true, strict: true, strictRequired: false });
addFormats(ajv);
ajv.addSchema(schema);
const validators = new Map(
  Object.keys(schema.$defs).map((name) => [name, ajv.compile({ $ref: `${schema.$id}#/$defs/${name}` })]),
);
export function validateDomain<K extends keyof DomainTypes>(name: K, value: unknown): asserts value is DomainTypes[K];
export function validateDomain(name: string, value: unknown): void;
export function validateDomain(name: string, value: unknown): void {
  const validate = validators.get(name);
  invariant(validate, `Unknown contract: ${name}`);
  invariant(validate(value), `${name}: ${ajv.errorsText(validate.errors)}`);
  const data = value as DomainTypes[keyof DomainTypes];
  switch (name) {
    case 'TelemetryEnvelope': {
      const envelope = data as DomainTypes['TelemetryEnvelope'];
      validateDomain('TelemetryFrame', envelope.payload);
      invariant(
        envelope.sessionId === envelope.payload.sessionId && envelope.sequence === envelope.payload.sequence,
        'Envelope identity mismatch',
      );
      break;
    }
    case 'SimulationConfig': {
      const config = data as DomainTypes['SimulationConfig'];
      invariant(
        (config.unavailableMetrics ?? []).every((id) => metricCatalog.has(id)),
        'Unknown unavailable metric',
      );
      break;
    }
    case 'Recording': {
      validateDomain('config' in data ? 'SimulationRecording' : 'RunRecording', data);
      break;
    }
    case 'RunRecording': {
      const recording = data as DomainTypes['RunRecording'];
      validateRecording(recording.metadata, recording.samples);
      break;
    }
    case 'SimulationRecording': {
      const recording = data as DomainTypes['SimulationRecording'];
      validateDomain('SimulationConfig', recording.config);
      invariant(
        recording.metadata.origin === 'simulated' && recording.metadata.seed === recording.config.seed,
        'Simulation manifest identity mismatch',
      );
      validateRecording(recording.metadata, recording.samples);
      invariant(
        recording.samples.length ===
          Math.ceil((recording.config.durationSeconds * 1000) / recording.config.sampleIntervalMs),
        'Simulation sample count mismatch',
      );
      recording.samples.forEach((sample, index) => {
        invariant(
          sample.telemetry.sequence === index &&
            sample.telemetry.elapsedUs === index * recording.config.sampleIntervalMs * 1000,
          'Simulation clock mismatch',
        );
        invariant(
          Date.parse(sample.telemetry.capturedAt) ===
            Date.parse(recording.config.epochUtc) + index * recording.config.sampleIntervalMs,
          'Simulation UTC mismatch',
        );
      });
      break;
    }
    case 'Measurement':
      validateMetrics([data as DomainTypes['Measurement']]);
      break;
    case 'TelemetryFrame': {
      const frame = data as TelemetryFrame;
      validateMetrics(frame.measurements, frame.origin);
      break;
    }
    case 'HardwareInventory': {
      const inventory = data as DomainTypes['HardwareInventory'];
      invariant(
        new Set(inventory.devices.map((d) => d.deviceId)).size === inventory.devices.length,
        'Duplicate device',
      );
      for (const device of inventory.devices) {
        validateMetrics(device.metrics, inventory.platform === 'simulated' ? 'simulated' : 'live');
        invariant(
          device.metrics.every((m) => m.deviceId === device.deviceId),
          'Inventory metric device mismatch',
        );
      }
      break;
    }
    case 'DeviceCapabilities': {
      const capabilities = data as DomainTypes['DeviceCapabilities'];
      invariant(
        new Set(capabilities.capabilities.map((c) => c.metricId)).size === capabilities.capabilities.length,
        'Duplicate capability',
      );
      for (const capability of capabilities.capabilities) {
        invariant(metricCatalog.has(capability.metricId), 'Unknown capability metric');
        invariant(
          capability.status === 'available' ? capability.reason === null : Boolean(capability.reason),
          'Capability reason mismatch',
        );
      }
      break;
    }
    case 'FlowState':
      validateFlow(data as DomainTypes['FlowState']);
      break;
    case 'ExperimentPhase':
      validateDomain('WorkloadDefinition', (data as DomainTypes['ExperimentPhase']).workload);
      break;
    case 'ExperimentDefinition': {
      const experiment = data as DomainTypes['ExperimentDefinition'];
      const first = experiment.phases[0]!;
      invariant(
        new Set(experiment.phases.map((p) => p.phaseId)).size === experiment.phases.length,
        'Duplicate phase ID',
      );
      experiment.phases.forEach((phase, index) => {
        invariant(phase.ordinal === index, 'Phase ordinals must be contiguous');
        validateDomain('ExperimentPhase', phase);
        for (const key of Object.keys(first.workload) as (keyof DomainTypes['WorkloadDefinition'])[]) {
          if (key !== 'definitionId' && key !== experiment.variable) {
            invariant(first.workload[key] === phase.workload[key], 'Experiment changed an uncontrolled variable');
          }
        }
      });
      break;
    }
    case 'WorkloadStatus': {
      const status = data as WorkloadStatus;
      if (status.state === 'completed') invariant(status.progress === 1, 'Completed progress must be one');
      if (['aborted', 'failed', 'cancelled', 'interrupted'].includes(status.state))
        invariant(status.reason, 'Terminal reason missing');
      break;
    }
    case 'RunMetadata': {
      const run = data as RunMetadata;
      invariant((run.outcome === 'running') === (run.endedAt === null), 'Terminal timestamp mismatch');
      if (run.endedAt) invariant(Date.parse(run.endedAt) >= Date.parse(run.startedAt), 'Run ends before start');
      if (!['running', 'completed'].includes(run.outcome)) invariant(run.abortReason, 'Abnormal outcome lacks reason');
      invariant(
        run.origin === 'simulated'
          ? run.seed !== null && run.simulatorVersion !== null
          : run.seed === null && run.simulatorVersion === null,
        'Simulator identity mismatch',
      );
      invariant((run.origin === 'simulated') === (run.inventory.platform === 'simulated'), 'Inventory origin mismatch');
      validateDomain('HardwareInventory', run.inventory);
      const devices = new Set(run.inventory.devices.map((d) => d.deviceId));
      for (const capability of run.capabilities) {
        validateDomain('DeviceCapabilities', capability);
        invariant(devices.has(capability.deviceId), 'Unknown capability device');
      }
      validateDomain('WorkloadDefinition', run.workload);
      validateMetrics(run.summaries, run.origin);
      break;
    }
    case 'ReplaySample': {
      const sample = data as ReplaySample;
      validateDomain('TelemetryFrame', sample.telemetry);
      validateDomain('WorkloadStatus', sample.workloadStatus);
      invariant(
        sample.runId === sample.telemetry.runId && sample.runId === sample.workloadStatus.runId,
        'Run identity mismatch',
      );
      invariant(
        sample.flow.sequence === sample.telemetry.sequence && sample.flow.elapsedUs === sample.telemetry.elapsedUs,
        'Flow/frame clock mismatch',
      );
      invariant(sample.workloadStatus.elapsedUs === sample.telemetry.elapsedUs, 'Status/frame clock mismatch');
      validateFlow(sample.flow, sample.telemetry);
      for (const event of [...sample.analyzerEvents, ...sample.safetyEvents]) {
        invariant(
          event.runId === sample.runId && event.elapsedUs <= sample.telemetry.elapsedUs,
          'Event identity/time mismatch',
        );
      }
      break;
    }
  }
}

const terminal = new Set(['completed', 'cancelled', 'aborted', 'failed', 'interrupted']);
const transitions: Record<WorkloadStatus['state'], readonly string[]> = {
  validating: ['validating', 'preparing', 'cancelled', 'aborted', 'failed', 'interrupted'],
  preparing: ['preparing', 'running', 'cancelled', 'aborted', 'failed', 'interrupted'],
  running: ['running', 'stopping', 'completed', 'cancelled', 'aborted', 'failed', 'interrupted'],
  stopping: ['stopping', 'completed', 'cancelled', 'aborted', 'failed', 'interrupted'],
  completed: ['completed'],
  cancelled: ['cancelled'],
  aborted: ['aborted'],
  failed: ['failed'],
  interrupted: ['interrupted'],
};
export function validateRecording(metadata: unknown, samples: unknown): void {
  validateDomain('RunMetadata', metadata);
  invariant(
    Array.isArray(samples) && samples.length > 0 && samples.length <= 6000,
    'Recording sample count outside bounds',
  );
  const devices = new Set(metadata.inventory.devices.map((d) => d.deviceId));
  let previous: ReplaySample | undefined;
  const evidenceFrames = new Map<number, TelemetryFrame>();
  for (const raw of samples) {
    validateDomain('ReplaySample', raw);
    const sample = raw;
    const frame = sample.telemetry;
    invariant(sample.runId === metadata.runId && frame.origin === metadata.origin, 'Recording origin/run mismatch');
    invariant(
      frame.measurements.every((m) => devices.has(m.deviceId)),
      'Unknown measurement device',
    );
    invariant(
      sample.flow.paths.every((p) => devices.has(p.from) && devices.has(p.to)) &&
        devices.has(sample.flow.queue.deviceId),
      'Unknown flow device',
    );
    if (previous) {
      invariant(frame.sessionId === previous.telemetry.sessionId, 'Session changed');
      invariant(
        frame.sequence > previous.telemetry.sequence && frame.elapsedUs >= previous.telemetry.elapsedUs,
        'Sample ordering invalid',
      );
      invariant(
        transitions[previous.workloadStatus.state].includes(sample.workloadStatus.state),
        'Invalid workload transition',
      );
    }
    evidenceFrames.set(frame.sequence, frame);
    for (const event of [...sample.analyzerEvents, ...sample.safetyEvents]) {
      for (const evidence of event.evidence) {
        const source = evidenceFrames.get(evidence.sequence);
        invariant(source, 'Event evidence does not resolve');
        resolveEvidence(source, evidence);
      }
    }
    previous = sample;
  }
  if (terminal.has(metadata.outcome))
    invariant(previous?.workloadStatus.state === metadata.outcome, 'Terminal outcome mismatch');
  else invariant(previous && !terminal.has(previous.workloadStatus.state), 'Running metadata contains terminal sample');
}
