import assert from 'node:assert/strict';
import { writeFile } from 'node:fs/promises';
import { validateDomain } from '../contracts/src/validate';
const bootstrap = (await fetch('http://127.0.0.1:8765/api/v1/bootstrap').then((r) => r.json())) as { token: string };
const socket = new WebSocket('ws://127.0.0.1:8765/api/v1/stream');
const frames: unknown[] = [];
const complete = new Promise<void>((resolve, reject) => {
  const timeout = setTimeout(() => reject(new Error('Telemetry stream timed out')), 8000);
  socket.onopen = () => socket.send(JSON.stringify({ token: bootstrap.token }));
  socket.onmessage = (event) => {
    try {
      const message = JSON.parse(String(event.data));
      validateDomain('TelemetryEnvelope', message);
      socket.send(JSON.stringify({ ack: message.sequence }));
      assert.equal(message.payload.origin, 'live');
      frames.push(message.payload);
      if (frames.length === 3) {
        clearTimeout(timeout);
        resolve();
      }
    } catch (e) {
      clearTimeout(timeout);
      reject(e);
    }
  };
  socket.onerror = () => reject(new Error('WebSocket failed'));
});
await complete;
socket.close();
await writeFile('out/live-telemetry.json', JSON.stringify(frames, null, 2));
console.log('PASS: three native LIVE frames over authenticated WebSocket; shared schema/semantic validation');
