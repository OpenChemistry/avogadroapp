// SPDX-License-Identifier: BSD-3-Clause
// PUPPETEER_MODULE may point to a temporary installation of puppeteer-core.
// Run inside an isolated display (e.g. xvfb-run); owns its own browser profile.
import assert from 'node:assert/strict';
import { mkdir, writeFile, mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
const { default: puppeteer } = await import(process.env.PUPPETEER_MODULE || 'puppeteer-core');
const url = process.argv[2] || 'http://127.0.0.1:8000';
const output = resolve(process.argv[3] || 'firefox-evidence');
await mkdir(output, { recursive: true });
const profile = await mkdtemp(join(tmpdir(), 'avogadro-firefox-'));
const browser = await puppeteer.launch({ browser: 'firefox', executablePath: process.env.FIREFOX_BINARY || '/usr/bin/firefox', headless: !process.env.DISPLAY,
  userDataDir: profile, extraPrefsFirefox: { 'webgl.force-enabled': true, 'gfx.webrender.software': true, 'browser.download.folderList': 2, 'browser.download.dir': output, 'browser.download.useDownloadDir': true, 'browser.helperApps.neverAsk.saveToDisk': 'text/plain' } });
const page = await browser.newPage();
const errors = [], checks = [];
page.on('pageerror', error => errors.push(error.message));
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const click = async id => { await page.$eval(`#${id}`, el => el.click()); await delay(150); };
const set = async (id, value) => { await page.select(`#${id}`, value); await delay(150); };
const count = async n => page.waitForFunction(n => document.querySelector('#counts').textContent.startsWith(`${n} atoms`), { timeout: 10000 }, n);
async function newDocument() {
  await click('new');
  await page.evaluate(() => { const dialog = document.querySelector('#discard-dialog'); if (dialog.open) dialog.querySelector('[value="discard"]').click(); });
  await count(0);
}
try {
  await page.setViewport({ width: 1200, height: 800 });
  await page.goto(url);
  await page.waitForFunction(() => !document.querySelector('#controls').disabled || document.querySelector('#status').dataset.error === 'true', { timeout: 90000 });
  assert.equal(await page.$eval('#controls', el => el.disabled), false, await page.$eval('#loading', el => el.textContent));
  checks.push('Firefox WASM startup');
  const rect = await page.$eval('#viewport', el => { const r = el.getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; });
  await page.mouse.click(rect.x, rect.y); await count(5);
  await page.screenshot({ path: join(output, 'draw.png') });
  await click('undo'); await count(0); await click('redo'); await count(5);
  checks.push('Draw, automatic hydrogens, undo/redo');
  await newDocument(); await set('element', '6'); await set('bond', '2'); await click('hydrogens');
  await page.mouse.move(rect.x - 50, rect.y); await page.mouse.down(); await page.mouse.move(rect.x + 70, rect.y, { steps: 12 }); await page.mouse.up();
  await count(2);
  assert.match(await page.$eval('#counts', el => el.textContent), /1 bonds/);
  checks.push('Draw bonded atoms with HTML settings');
  // Keyboard input in HTML controls must not invoke a viewport edit.
  await page.focus('#element'); await page.keyboard.press('Backspace'); await page.keyboard.down('Control'); await page.keyboard.press('z'); await page.keyboard.up('Control'); await count(2);
  checks.push('HTML focus isolates shortcuts');
  await page.$eval('[data-tool="select"]', el => el.click());
  await page.mouse.move(rect.x - 200, rect.y - 200); await page.mouse.down(); await page.mouse.move(rect.x + 200, rect.y + 200, { steps: 12 }); await page.mouse.up();
  await click('delete'); await count(0); await click('undo'); await count(2);
  checks.push('Selection, grouped deletion and undo');
  await page.$eval('[data-tool="navigate"]', el => el.click());
  await page.mouse.move(rect.x, rect.y); await page.mouse.down(); await page.mouse.move(rect.x + 70, rect.y + 50, { steps: 8 }); await page.mouse.up();
  await page.mouse.down({ button: 'right' }); await page.mouse.move(rect.x + 90, rect.y + 60, { steps: 8 }); await page.mouse.up({ button: 'right' });
  await page.mouse.wheel({ deltaY: 100 }); await click('reset');
  await page.setViewport({ width: 950, height: 700 }); await delay(300);
  checks.push('Rotate, pan, zoom, fit and resize');
  await newDocument();
  const good = join(profile, 'records.xyz'), bad = join(profile, 'bad.cjson');
  await writeFile(good, '1\nfirst\nO 0 0 0\n1\nsecond\nN 1 1 1\n'); await writeFile(bad, 'invalid json');
  await (await page.$('#file')).uploadFile(good); await count(1);
  await page.waitForFunction(() => document.querySelector('#status').textContent.includes('first molecule'));
  await (await page.$('#file')).uploadFile(bad);
  await page.waitForFunction(() => document.querySelector('#status').dataset.error === 'true'); await count(1);
  checks.push('File input, first-record notice and failed-import preservation');
  // Verify exported browser Blob payloads without relying on download UI.
  await page.evaluate(() => { window.exported = []; const create = URL.createObjectURL; URL.createObjectURL = blob => { window.exported.push(blob.text()); return create(blob); }; });
  for (const format of ['cjson', 'mol', 'sdf', 'xyz']) { await set('format', format); await click('download'); }
  const exported = await page.evaluate(() => Promise.all(window.exported));
  assert.equal(exported.length, 4); assert.deepEqual(JSON.parse(exported[0]).atoms.elements.number, [8]);
  assert.ok(exported[1].includes('V2000')); assert.ok(exported[2].includes('$$$$')); assert.ok(exported[3].startsWith('1\n'));
  checks.push('CJSON, MOL, SDF and XYZ browser export payloads');
  await page.screenshot({ path: join(output, 'loaded.png') });
  assert.deepEqual(errors, []);
  await writeFile(join(output, 'result.json'), JSON.stringify({ checks, errors }, null, 2));
  console.log(JSON.stringify({ passed: checks.length, checks }));
} catch (error) {
  await page.screenshot({ path: join(output, 'failure.png') }).catch(() => {});
  throw error;
} finally { await browser.close(); await rm(profile, { recursive: true, force: true }); }
