import { chromium } from '@playwright/test';
import { writeFile } from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  await page.goto('http://127.0.0.1:5174');
  await page.getByRole('button', { name: 'Simulation', exact: true }).click();
  const cdp = await page.context().newCDPSession(page);
  const samples = [];
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  for (let minute = 0; minute <= 32; minute++) {
    if (minute) await new Promise((resolve) => setTimeout(resolve, 60000));
    await cdp.send('HeapProfiler.collectGarbage');
    const heap = await cdp.send('Runtime.getHeapUsage');
    samples.push({ minute, usedSize: heap.usedSize });
    await page.getByRole('button', { name: 'Simulation', exact: true }).click();
    await writeFile('out/stability-soak.json', JSON.stringify({ complete: minute === 32, samples, errors }, null, 2));
    console.log('Retained heap minute', minute, heap.usedSize);
  }
  const growth = (samples.at(-1).usedSize - samples[2].usedSize) / samples[2].usedSize;
  if (errors.length || growth >= 0.1) throw new Error(`Stability failed: growth=${growth}; ${errors.join(';')}`);
  console.log('PASS: 30 minutes after warmup; retained heap growth', growth);
} finally {
  await browser.close();
}
