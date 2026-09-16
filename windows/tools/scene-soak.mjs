import { chromium } from '@playwright/test';
import { writeFile } from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
await page.goto('http://127.0.0.1:5173');
await page.getByRole('button', { name: 'Simulation', exact: true }).click();
await page.waitForFunction(() => window.ioscopeSceneStats);
await page.getByRole('button', { name: 'GPU', exact: true }).click();
await page.getByRole('button', { name: /Focus component/ }).click();
await page.getByRole('button', { name: 'Isolate', exact: true }).click();
await page.getByRole('button', { name: 'Show system', exact: true }).click();
await page.getByRole('button', { name: 'Data path', exact: true }).click();
await page.getByRole('button', { name: 'Physical', exact: true }).click();
await page.getByRole('button', { name: 'Reset camera', exact: true }).click();
const report = await page.evaluate(async () => {
  const deltas = [];
  let last = performance.now();
  const start = last;
  return await new Promise((resolve) => {
    function frame(now) {
      deltas.push(now - last);
      last = now;
      if (now - start < 300000) requestAnimationFrame(frame);
      else {
        deltas.sort((a, b) => a - b);
        resolve({
          durationMs: now - start,
          frames: deltas.length,
          p95Ms: deltas[Math.floor(deltas.length * 0.95)],
          averageFps: (deltas.length / (now - start)) * 1000,
          scene: window.ioscopeSceneStats,
          heap: performance.memory?.usedJSHeapSize,
        });
      }
    }
    requestAnimationFrame(frame);
  });
});
await writeFile('out/scene-performance.json', JSON.stringify(report, null, 2));
console.log(report);
await browser.close();
