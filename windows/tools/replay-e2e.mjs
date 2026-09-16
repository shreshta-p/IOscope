import { chromium, expect } from '@playwright/test';
const browser = await chromium.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true,
});
const base = 'http://127.0.0.1:8765/api/v1';
const { token } = await fetch(base + '/bootstrap').then((r) => r.json());
let id;
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  await page.goto('http://127.0.0.1:5173');
  await page.getByRole('button', { name: 'Simulation', exact: true }).click();
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  await page.getByRole('button', { name: /^Runs/ }).first().click();
  const saved = page.waitForResponse((r) => r.url().endsWith('/recordings') && r.request().method() === 'POST');
  await page.getByRole('button', { name: 'Save current run', exact: true }).click();
  const response = await saved;
  expect(response.status()).toBe(200);
  id = (await response.json()).id;
  await page.reload();
  await page.getByRole('button', { name: /^Runs/ }).first().click();
  const row = page.locator('.run-row').filter({ hasText: id.slice(0, 8) });
  await expect(row).toBeVisible();
  await row.getByRole('button', { name: /Replay/ }).click();
  await expect(page.locator('.source-badge')).toHaveText(/RECORDED.*SIMULATED/);
  await expect(page.getByRole('button', { name: 'Play', exact: true })).toBeVisible();
  const slider = page.getByRole('slider', { name: 'Replay timeline' });
  await slider.fill('30');
  await expect(slider).toHaveValue('30');
  await page.getByRole('button', { name: 'Previous sample' }).click();
  await expect(slider).toHaveValue('29');
  await page.getByRole('button', { name: 'Next sample' }).click();
  await expect(slider).toHaveValue('30');
  await page.getByRole('combobox', { name: 'Playback speed' }).selectOption('4');
  await slider.fill('59');
  await page.getByRole('button', { name: 'Play', exact: true }).click();
  await expect(slider).toHaveValue('0');
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  console.log('PASS: SQLite save survives reload; replay preserves origin; seek/step/speed/end restart');
} finally {
  if (id) await fetch(base + '/recordings/' + id, { method: 'DELETE', headers: { Authorization: 'Bearer ' + token } });
  await browser.close();
}
