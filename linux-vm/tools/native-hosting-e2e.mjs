import { chromium } from '@playwright/test';
const origin = 'http://127.0.0.1:8765';
const get = async (path) => {
  try {
    return await fetch(origin + path, { signal: AbortSignal.timeout(8000) });
  } catch (error) {
    throw new Error(`Native request failed at ${path}: ${error.message}`);
  }
};
const response = await get('/');
if (response.status !== 200) throw new Error(`Native homepage returned ${response.status}, expected 200`);
if (!response.headers.get('content-type')?.includes('text/html')) throw new Error('Homepage MIME type is not HTML');
const html = await response.text();
const assets = [...html.matchAll(/(?:src|href)="(\/assets\/[^"?]+)"/g)].map((match) => match[1]);
if (assets.length < 2) throw new Error('Built scripts/styles missing');
for (const path of assets) {
  const asset = await get(path);
  await asset.arrayBuffer();
  if (asset.status !== 200) throw new Error(`Native asset failed: ${path} (${asset.status})`);
  if (path.endsWith('.css') && !asset.headers.get('content-type')?.includes('text/css'))
    throw new Error('Stylesheet MIME type wrong');
}
for (const path of ['/assets/missing-file.js', '/assets/%2Fetc%2Fpasswd', '/assets/nested%2F..%2F..%2Fetc%2Fpasswd']) {
  const denied = await get(path);
  await denied.arrayBuffer();
  if (denied.ok) throw new Error('Invalid asset path accepted: ' + path);
}
const browser = await chromium.launch({ headless: true, channel: 'chromium', args: ['--no-sandbox'] });
try {
  const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(origin);
  await page.getByRole('heading', { name: 'System overview' }).waitFor();
  await page.getByRole('button', { name: /^Workload Lab/ }).click();
  await page.getByText('Required disk reserve', { exact: true }).waitFor();
  await page.screenshot({ path: 'out/native-hosting.png', fullPage: true });
  if (errors.length) throw new Error(errors.join('\n'));
  console.log('PASS: native homepage, JS/CSS assets, rejected asset paths, mounted Workload Lab; no workload started');
} finally {
  await browser.close();
}
