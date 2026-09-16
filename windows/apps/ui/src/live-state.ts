import type { TelemetryFrame } from '../../../contracts/src/index';
export function liveContinuity(previous: TelemetryFrame | null, next: TelemetryFrame): string | null {
  if (next.origin !== 'live') throw new Error('Synthetic data rejected from live stream');
  if (!previous) return null;
  if (previous.sessionId !== next.sessionId) return 'Agent restarted · new telemetry session';
  if (next.sequence <= previous.sequence || next.elapsedUs < previous.elapsedUs)
    throw new Error('Out-of-order live frame');
  return next.sequence > previous.sequence + 1 ? 'Stream gap · showing the latest measured snapshot' : null;
}
