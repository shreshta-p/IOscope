import mapping from '../flow-semantics.v1.json';
import type { Evidence, FlowState, TelemetryFrame } from './index';
import { invariant } from './metrics';

export const flowSemantics = mapping;
export function bandwidthActivity(bytesPerSecond: number): number {
  invariant(Number.isFinite(bytesPerSecond) && bytesPerSecond >= 0, 'Invalid flow bandwidth');
  const scale = mapping.bandwidthScaleBytesPerSecond;
  return Math.log2(1 + Math.min(bytesPerSecond, scale) / 1048576) / Math.log2(1 + scale / 1048576);
}
export function resolveEvidence(frame: TelemetryFrame, evidence: Evidence) {
  invariant(frame.sequence === evidence.sequence, 'Evidence sequence mismatch');
  const matches = frame.measurements.filter(
    (m) => m.deviceId === evidence.deviceId && m.metricId === evidence.metricId,
  );
  // v1 evidence has no scope field: ambiguous scoped metrics must be rejected.
  invariant(matches.length === 1, 'Evidence missing or scope ambiguous');
  const metric = matches[0]!;
  invariant(
    metric.status === 'available' && metric.value !== null && metric.ageMs <= mapping.staleAfterMs,
    'Evidence is unavailable or stale',
  );
  return metric;
}
export function validateFlow(flow: FlowState, frame?: TelemetryFrame): void {
  const ids = new Set<string>();
  for (const path of flow.paths) {
    invariant(!ids.has(path.pathId), 'Duplicate flow path');
    ids.add(path.pathId);
    invariant(
      path.status === 'active' ? path.activity > 0 && path.evidence.length > 0 : path.activity === 0,
      'Flow activity/status mismatch',
    );
    if (!frame) continue;
    path.evidence.forEach((e) => resolveEvidence(frame, e));
    const spec = mapping.paths.find((p) => p.pathId === path.pathId);
    if (spec && path.status !== 'unknown') {
      const evidence = path.evidence.find((e) => e.metricId === spec.metricId);
      invariant(evidence, 'Flow lacks source evidence');
      const metric = resolveEvidence(frame, evidence);
      invariant(
        metric.deviceId === (spec.pathId === 'nvme-ram-read' ? path.from : path.to),
        'Storage path evidence device mismatch',
      );
      invariant(Math.abs(path.activity - bandwidthActivity(metric.value!)) < 1e-12, 'Flow scaling mismatch');
    }
    const pipeline = mapping.pipelinePaths.find((p) => p.pathId === path.pathId);
    if (pipeline && path.status !== 'unknown') {
      const stageEvidence = path.evidence.find((e) => e.metricId === 'pipeline.stage');
      const activityEvidence = path.evidence.find((e) => e.metricId === pipeline.metricId);
      invariant(stageEvidence && activityEvidence, 'Pipeline flow lacks stage/activity evidence');
      const stage = resolveEvidence(frame, stageEvidence);
      const activity = resolveEvidence(frame, activityEvidence);
      const expected =
        stage.value === pipeline.stage
          ? pipeline.scaling === 'bandwidth'
            ? bandwidthActivity(activity.value!)
            : activity.value! / 100
          : 0;
      invariant(Math.abs(path.activity - expected) < 1e-12, 'Pipeline activity/stage mismatch');
    }
  }
  if (flow.queue.observedAverage !== null) {
    invariant(flow.queue.evidence.length > 0, 'Queue lacks evidence');
    if (frame) {
      const evidence = flow.queue.evidence.find(
        (e) => e.metricId === 'storage.queue.average' && e.deviceId === flow.queue.deviceId,
      );
      invariant(evidence, 'Queue lacks matching evidence');
      invariant(resolveEvidence(frame, evidence).value === flow.queue.observedAverage, 'Queue value mismatch');
    }
  }
}
