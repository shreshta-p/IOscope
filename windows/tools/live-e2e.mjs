import { chromium, expect } from '@playwright/test';
import { spawn } from 'node:child_process';
import { resolve } from 'node:path';
import { writeFile } from 'node:fs/promises';
const executable = resolve('build/native-vs16/Release/ioscope_agent.exe');
let child;
async function start() {
  child = spawn(executable, ['.'], { cwd: process.cwd(), windowsHide: true, stdio: 'ignore' });
  for (let attempt = 0; attempt < 30; attempt++) {
    try {
      const response = await fetch('http://127.0.0.1:8765/api/v1/bootstrap');
      if (response.ok) return;
    } catch {}
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error('Agent startup timed out');
}
async function stop() {
  if (child && child.exitCode === null) {
    const ended = new Promise((r) => child.once('exit', r));
    child.kill();
    await ended;
  }
}
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
try {
  await start();
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  await page.goto('http://127.0.0.1:5173');
  await expect(page.locator('footer')).toContainText('LIVE TELEMETRY', { timeout: 15000 });
  await expect(page.locator('.source-badge')).toHaveText('LIVE');
  const cpu = page.locator('.system-strip strong').first();
  await expect(cpu).not.toHaveText('Unavailable');
  await page.screenshot({ path: 'out/live-ui.png', fullPage: true });
  await stop();
  await expect(page.locator('footer')).toContainText('not connected', { timeout: 7000 });
  await expect(cpu).toHaveText('Unavailable');
  await start();
  await expect(page.locator('footer')).toContainText('LIVE TELEMETRY', { timeout: 15000 });
  await expect(cpu).not.toHaveText('Unavailable');
  const { token } = await fetch('http://127.0.0.1:8765/api/v1/bootstrap').then((r) => r.json());
  const headers = { Authorization: 'Bearer ' + token };
  const inventory = await fetch('http://127.0.0.1:8765/api/v1/inventory', { headers }).then((r) => r.json());
  await writeFile('out/live-inventory.json', JSON.stringify(inventory, null, 2));
  console.log('PASS: real LIVE UI; disconnect clears metrics; restarted agent reconnects with fresh token');
} finally {
  await stop();
  await browser.close();
}
