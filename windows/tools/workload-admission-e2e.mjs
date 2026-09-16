import { chromium } from '@playwright/test';
import { writeFile } from 'node:fs/promises';
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto('http://127.0.0.1:5173');
  await page.getByRole('button', { name: /^Workload Lab/ }).click();
  await page.getByRole('heading', { name: 'A bounded storage trial' }).waitFor();
  const response = await page.waitForResponse(
    (value) => value.url().endsWith('/runs/admission') && value.status() === 200,
  );
  const admission = await response.json();
  await writeFile('out/workload-admission.json', JSON.stringify(admission, null, 2));
  // This test is read-only even on a machine that passes admission. Never click Start.
  await page.getByText('Required disk reserve', { exact: true }).waitFor();
  if (!admission.allowed && (await page.getByRole('button', { name: 'Start native workload' }).isEnabled()))
    throw new Error('Denied workload has enabled start');
  await page.getByLabel('Outstanding request limit').selectOption('4');
  await page.waitForResponse(
    async (value) =>
      value.url().endsWith('/runs/admission') &&
      value.status() === 200 &&
      (await value.json()).workload.queueDepth === 4,
  );
  await page.screenshot({ path: 'out/workload-lab.png', fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth);
  if (overflow) throw new Error('Workload Lab overflows mobile viewport');
  if (errors.length) throw new Error(errors.join('\n'));
  console.log('PASS: native admission, denied-start state, control update, mobile layout; no workload started');
} finally {
  await browser.close();
}
