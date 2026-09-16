import { chromium } from '@playwright/test';
import { mkdir } from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
await page.goto('http://127.0.0.1:5173');
await page.getByRole('heading', { name: 'System overview' }).waitFor();
await page.getByRole('button', { name: 'Simulation', exact: true }).click();
await page.getByRole('button', { name: 'Pause', exact: true }).click();
for (const name of ['Workload Lab', 'Experiments', 'Runs', 'Learn', 'Analyze']) {
  await page
    .getByRole('button', { name: new RegExp('^' + name) })
    .first()
    .click();
  await page.getByRole('heading', { name, exact: true }).waitFor();
}
await page.getByRole('button', { name: /^Live/ }).first().click();
await mkdir('out', { recursive: true });
await page.screenshot({ path: 'out/ui-smoke.png', fullPage: true });
if (errors.length) throw new Error(errors.join('\n'));
console.log('PASS: six routes, simulation/pause, no browser exceptions');
await browser.close();
