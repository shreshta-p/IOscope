import catalog from '../metric-catalog.v1.json';
import type { Measurement, TelemetryFrame } from './index';

export const metricCatalog = new Map(catalog.metrics.map((metric) => [metric.metricId, metric]));
export function invariant(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}
export function validateMetrics(metrics: Measurement[], origin?: TelemetryFrame['origin']): void {
  const seen = new Set<string>();
  for (const metric of metrics) {
    const definition = metricCatalog.get(metric.metricId);
    invariant(definition, `Unknown metric: ${metric.metricId}`);
    invariant(metric.unit === definition.unit, 'Metric unit mismatch');
    invariant(metric.provenance !== 'conceptual', 'Physical metrics cannot be conceptual');
    if (origin)
      invariant(
        origin === 'simulated'
          ? metric.provenance === 'simulated'
          : metric.provenance === 'measured' || metric.provenance === 'derived',
        'Origin/provenance mismatch',
      );
    const key = JSON.stringify([metric.deviceId, metric.metricId, metric.scope]);
    invariant(!seen.has(key), 'Duplicate metric key');
    seen.add(key);
    if (metric.value !== null) {
      invariant(
        Number.isFinite(metric.value) &&
          metric.value >= definition.min &&
          (definition.max === null || metric.value <= definition.max),
        'Metric outside bounds',
      );
    }
  }
  for (const [part, total] of [
    ['ram.available', 'ram.total'],
    ['vram.used', 'vram.total'],
  ]) {
    for (const value of metrics.filter((m) => m.metricId === part && m.value !== null)) {
      const capacity = metrics.find(
        (m) => m.metricId === total && m.deviceId === value.deviceId && m.scope === value.scope,
      );
      if (capacity?.value !== null && capacity?.value !== undefined) {
        invariant(value.value! <= capacity.value, 'Used/available capacity exceeds total');
      }
    }
  }
}
