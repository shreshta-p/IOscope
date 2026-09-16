import type { Recording } from '../../../contracts/src/index';
export function compareMetric(a: Recording, b: Recording, id: string) {
  const result: {
    unit: string;
    means: (number | null)[];
    counts: number[];
    delta: number | null;
    percent: number | null;
    reason: string | null;
  } = { unit: '', means: [null, null], counts: [0, 0], delta: null, percent: null, reason: null };
  if (a.metadata.origin !== b.metadata.origin) {
    result.reason = 'Different evidence origins';
    return result;
  }
  if (id.includes('.p95') || id.includes('.p99')) {
    result.reason = 'A percentile cannot be averaged';
    return result;
  }
  const groups = [a, b].map((r) => r.samples.flatMap((s) => s.telemetry.measurements.filter((m) => m.metricId === id)));
  const signatures = new Set(groups.flat().map((m) => JSON.stringify([m.unit, m.scope, m.windowMs, m.deviceId])));
  if (signatures.size > 1) {
    result.reason = 'Incompatible unit, scope, window or device';
    return result;
  }
  result.unit = groups[0]?.[0]?.unit ?? '';
  result.means = groups.map((g, i) => {
    const values = g
      .filter((m) => m.status === 'available' && m.ageMs <= 3000 && m.value !== null)
      .map((m) => m.value!);
    result.counts[i] = values.length;
    return values.length ? values.reduce((a, b) => a + b, 0) / values.length : null;
  });
  const [first, second] = result.means;
  if (first != null && second != null) {
    result.delta = second - first;
    result.percent = first === 0 ? null : (100 * result.delta) / first;
  } else result.reason = 'Insufficient available samples';
  return result;
}
