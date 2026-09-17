import { useEffect, useState, type Dispatch, type SetStateAction } from 'react';
import { connect, api } from './client';
import { validateDomain } from '../../../contracts/src/validate';
import type { Session } from './session';
import type { TelemetryFrame } from '../../../contracts/src/index';
import { liveContinuity } from './live-state';
export function useLive(setSession: Dispatch<SetStateAction<Session>>) {
  const [connected, setConnected] = useState(false);
  useEffect(() => {
    let disposed = false,
      socket: WebSocket | null = null,
      retry: ReturnType<typeof setTimeout> | undefined;
    let lastFrame = 0;
    let previous: TelemetryFrame | null = null;
    const retryConnection = () => {
      if (disposed) return;
      setConnected(false);
      setSession((s) => ({ ...s, live: null }));
      retry = setTimeout(() => void open(), 2000);
    };
    async function open() {
      try {
        const info = await connect();
        const inventory = await api('/inventory');
        validateDomain('HardwareInventory', inventory);
        if (disposed) return;
        setSession((s) => ({ ...s, inventory }));
        socket = new WebSocket(`ws://${window.location.host}/api/v1/stream`);
        socket.onopen = () => socket?.send(JSON.stringify({ token: info.token }));
        socket.onmessage = (event) => {
          try {
            const envelope: unknown = JSON.parse(String(event.data));
            validateDomain('TelemetryEnvelope', envelope);
            if (envelope.kind === 'telemetry') {
              socket?.send(JSON.stringify({ ack: envelope.sequence }));
              const frame = envelope.payload;
              const liveGap = liveContinuity(previous, frame);
              previous = frame;
              lastFrame = Date.now();
              setSession((s) => ({ ...s, live: frame, liveGap }));
              setConnected(true);
            }
          } catch {
            socket?.close(1008, 'Invalid telemetry');
          }
        };
        socket.onclose = retryConnection;
        socket.onerror = () => socket?.close();
      } catch {
        retryConnection();
      }
    }
    void open();
    const stale = setInterval(() => {
      if (lastFrame && Date.now() - lastFrame > 3000) {
        setConnected(false);
        setSession((s) => (s.live ? { ...s, live: null } : s));
      }
    }, 500);
    return () => {
      disposed = true;
      clearInterval(stale);
      if (retry) clearTimeout(retry);
      socket?.close();
    };
  }, [setSession]);
  return connected;
}
