import type { TelemetryFrame } from '../../../contracts/src/index';
export function metric(frame: TelemetryFrame | null, id: string): number | null {
  const m = frame?.measurements.find((m) => m.metricId === id);
  return m?.status === 'available' && m.ageMs <= 3000 ? m.value : null;
}
export function display(value: number | null, unit: string): string {
  if (value === null) return 'Unavailable';
  if (unit === 'bytes/s') return `${(value / 1048576).toFixed(1)} MiB/s`;
  if (unit === 'bytes') return `${(value / 1073741824).toFixed(1)} GiB`;
  return `${value.toFixed(unit === 'count' ? 1 : unit === 'ms' ? 2 : 0)} ${unit === 'percent' ? '%' : unit === 'celsius' ? '°C' : unit}`;
}
export const components = [
  { id: 'cpu0', name: 'CPU', detail: 'Compute & scheduling', metrics: ['cpu.utilization', 'cpu.temperature'] },
  { id: 'ram0', name: 'System RAM', detail: 'Host memory', metrics: ['ram.available', 'ram.total'] },
  {
    id: 'nvme0',
    name: 'NVMe SSD',
    detail: 'Storage subsystem',
    metrics: [
      'storage.read.bytes_per_second',
      'storage.write.bytes_per_second',
      'storage.queue.average',
      'storage.latency.mean',
      'storage.temperature',
    ],
  },
  {
    id: 'gpu0',
    name: 'GPU',
    detail: 'Parallel compute',
    metrics: ['gpu.utilization', 'gpu.temperature', 'gpu.clock', 'gpu.power'],
  },
  { id: 'vram0', name: 'VRAM', detail: 'Device memory', metrics: ['vram.used', 'vram.total'] },
];
export const metricNames: Record<string, string> = {
  'cpu.utilization': 'Utilization',
  'cpu.temperature': 'Temperature',
  'ram.available': 'Available memory',
  'ram.total': 'Total memory',
  'storage.read.bytes_per_second': 'Read throughput',
  'storage.write.bytes_per_second': 'Write throughput',
  'storage.queue.average': 'Observed queue average',
  'storage.latency.mean': 'Mean latency',
  'storage.temperature': 'Temperature',
  'gpu.utilization': 'Utilization',
  'gpu.temperature': 'Temperature',
  'gpu.clock': 'Graphics clock',
  'gpu.power': 'Power',
  'vram.used': 'Allocated memory',
  'vram.total': 'Total memory',
};
