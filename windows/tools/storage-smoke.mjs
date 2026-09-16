import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
const url = 'http://127.0.0.1:8765/api/v1';
const bootstrap = await fetch(url + '/bootstrap').then((r) => r.json());
const headers = { Authorization: 'Bearer ' + bootstrap.token, 'Content-Type': 'application/json' };
assert.equal((await fetch(url + '/recordings')).status, 401);
assert.equal((await fetch(url + '/bootstrap', { headers: { Origin: 'https://malicious.example' } })).status, 403);
const recording = JSON.parse(await readFile('simulation/fixtures/seed-42.json', 'utf8'));
const response = await fetch(url + '/recordings', { method: 'POST', headers, body: JSON.stringify(recording) });
assert.equal(response.status, 200, await response.clone().text());
const { id } = await response.json();
try {
  assert.deepEqual(await fetch(url + '/recordings/' + id, { headers }).then((r) => r.json()), recording);
  assert((await fetch(url + '/recordings', { headers }).then((r) => r.json())).some((r) => r.id === id));
  const original = JSON.stringify(recording);
  recording.samples[0].telemetry.measurements[0].unit = 'ms';
  assert.equal(
    (await fetch(url + '/recordings', { method: 'POST', headers, body: JSON.stringify(recording) })).status,
    400,
  );
  for (const mutate of [
    (r) => (r.samples[0].flow.paths[0].activity = 0.99),
    (r) => (r.samples[0].flow.queue.observedAverage = 32),
    (r) => (r.samples[1].telemetry.sessionId = 'wrong-session'),
    (r) => r.samples.pop(),
  ]) {
    const changed = JSON.parse(original);
    mutate(changed);
    assert.equal(
      (await fetch(url + '/recordings', { method: 'POST', headers, body: JSON.stringify(changed) })).status,
      400,
    );
  }
} finally {
  assert.equal((await fetch(url + '/recordings/' + id, { method: 'DELETE', headers })).status, 200);
}
console.log('PASS: native token/origin checks; durable save/list/read/delete; corrupt unit rejection');
