import type { Evidence, FlowState, Measurement, TelemetryFrame } from '../../contracts/src/index';
import { bandwidthActivity, flowSemantics } from '../../contracts/src/flow-mapping';
import { validateDomain } from '../../contracts/src/validate';

export function deriveFlow(frame: TelemetryFrame, configuredLimit: number | null = null): FlowState {
  validateDomain('TelemetryFrame', frame);
  const find = (metricId: string) => {
    const matches = frame.measurements.filter((m) => m.metricId === metricId);
    const metric = matches.length === 1 ? matches[0] : undefined;
    return metric?.status === 'available' && metric.value !== null && metric.ageMs <= flowSemantics.staleAfterMs
      ? metric
      : undefined;
  };
  const evidence = (metric: Measurement): Evidence => ({
    sequence: frame.sequence,
    deviceId: metric.deviceId,
    metricId: metric.metricId,
  });
  const paths: FlowState['paths'] = flowSemantics.paths.map((spec) => {
    const metric = find(spec.metricId);
    const read = spec.pathId === 'nvme-ram-read';
    const activity = metric ? bandwidthActivity(metric.value!) : 0;
    return {
      pathId: spec.pathId,
      from: read ? 'nvme0' : 'ram0',
      to: read ? 'ram0' : 'nvme0',
      representation: 'aggregated-conceptual',
      status: !metric ? 'unknown' : activity > 0 ? 'active' : 'idle',
      activity,
      direction: 'forward',
      evidence: metric ? [evidence(metric)] : [],
    };
  });
  const stage = find('pipeline.stage');
  for (const spec of flowSemantics.pipelinePaths) {
    const metric = find(spec.metricId);
    const known = stage !== undefined && metric !== undefined;
    const activity =
      known && stage.value === spec.stage
        ? spec.scaling === 'bandwidth'
          ? bandwidthActivity(metric.value!)
          : metric.value! / 100
        : 0;
    paths.push({
      pathId: spec.pathId,
      from: spec.from,
      to: spec.to,
      representation: 'aggregated-conceptual',
      status: !known ? 'unknown' : activity > 0 ? 'active' : 'idle',
      activity,
      direction: 'forward',
      evidence: known ? [evidence(stage), evidence(metric)] : [],
    });
  }
  const observed = find('storage.queue.average');
  const flow: FlowState = {
    schemaVersion: '1.0.0',
    mappingVersion: '1.0.0',
    sequence: frame.sequence,
    elapsedUs: frame.elapsedUs,
    paths,
    queue: {
      deviceId: 'nvme0',
      representation: 'aggregated',
      configuredLimit,
      observedAverage: observed?.value ?? null,
      evidence: observed ? [evidence(observed)] : [],
    },
  };
  validateDomain('FlowState', flow);
  return flow;
}
