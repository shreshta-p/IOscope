import type { Recording, TelemetryFrame, HardwareInventory } from '../../../contracts/src/index';
export interface Session {
  mode: 'LIVE' | 'SIMULATED' | 'RECORDED';
  recording: Recording | null;
  index: number;
  playing: boolean;
  speed: number;
  live: TelemetryFrame | null;
  inventory?: HardwareInventory;
  liveGap?: string | null;
}
export function sampleAt(recording: Recording, milliseconds: number): number {
  if (!Number.isFinite(milliseconds)) throw new Error('Invalid playback time');
  let lo = 0,
    hi = recording.samples.length - 1;
  while (lo < hi) {
    const mid = Math.ceil((lo + hi) / 2);
    if (recording.samples[mid]!.telemetry.elapsedUs <= milliseconds * 1000) lo = mid;
    else hi = mid - 1;
  }
  return lo;
}
export function changeSource(session: Session, mode: Session['mode'], recording?: Recording): Session {
  if (mode === 'SIMULATED' && recording?.metadata.origin !== 'simulated')
    throw new Error('Live recordings cannot become simulation');
  if (mode !== 'LIVE' && !recording) throw new Error('A recording is required');
  return {
    ...session,
    mode,
    recording: mode === 'LIVE' ? null : recording!,
    index: 0,
    playing: mode === 'SIMULATED',
    live: null,
  };
}

export const recordingLabel = (r: Recording) => ('config' in r ? r.config.scenario : r.metadata.workload.definitionId);
export const recordingDuration = (r: Recording | null) =>
  r
    ? 'config' in r
      ? r.config.durationSeconds
      : (r.samples.at(-1)!.telemetry.elapsedUs - r.samples[0]!.telemetry.elapsedUs) / 1e6
    : 0;
export function playbackDelay(r: Recording, index: number, speed: number) {
  const a = r.samples[index],
    b = r.samples[index + 1];
  return a && b ? Math.max(1, (b.telemetry.elapsedUs - a.telemetry.elapsedUs) / 1000 / speed) : 0;
}
