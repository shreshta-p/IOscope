import assert from 'node:assert/strict';
const base = 'http://127.0.0.1:8765/api/v1';
const { token } = (await fetch(base + '/bootstrap').then((r) => r.json())) as { token: string };
const headers = { Authorization: 'Bearer ' + token, 'Content-Type': 'application/json' };
for (const body of ['{"a":1,"a":2}', '['.repeat(66) + '0' + ']'.repeat(66)])
  assert.equal((await fetch(base + '/recordings', { method: 'POST', headers, body })).status, 400);
async function closed(auth: string | null) {
  const socket = new WebSocket('ws://127.0.0.1:8765/api/v1/stream');
  let frames = 0;
  await new Promise<void>((resolve, reject) => {
    const timeout = setTimeout(() => {
      socket.close();
      reject(new Error('Stream deadline failed'));
    }, 7000);
    socket.onopen = () => {
      if (auth) socket.send(JSON.stringify({ token: auth }));
    };
    socket.onmessage = () => frames++;
    socket.onclose = () => {
      clearTimeout(timeout);
      resolve();
    };
    socket.onerror = () => {
      clearTimeout(timeout);
      reject(new Error('Unexpected stream error'));
    };
  });
  return frames;
}
assert.equal(await closed(null), 0);
assert.equal(await closed('a'.repeat(48)), 0);
assert.equal(await closed(token), 1);
console.log('PASS: bounded JSON, unauthenticated deadline, wrong token, single outstanding frame under backpressure');
