// SPDX-License-Identifier: BSD-3-Clause
// Run: PUPPETEER_MODULE=... node startup-failures.mjs STAGED_DIRECTORY
// Owns a loopback server and a fresh headless Chrome profile.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { basename, join, resolve } from 'node:path';
const { default: puppeteer } = await import(process.env.PUPPETEER_MODULE || 'puppeteer-core');
const directory = resolve(process.argv[2]);
let scenario;
const server = createServer(async (request, response) => {
  if (scenario !== 'isolation') {
    response.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    response.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
  }
  const name = basename(new URL(request.url, 'http://localhost').pathname) || 'index.html';
  const types = { js: 'text/javascript', wasm: 'application/wasm', css: 'text/css', html: 'text/html' };
  response.setHeader('Content-Type', types[name.split('.').pop()] || 'text/plain');
  try {
    if (scenario === 'missing-wasm' && name.endsWith('.wasm')) throw new Error('intentionally absent');
    response.end(await readFile(join(directory, name)));
  } catch { response.statusCode = 404; response.end('Missing asset'); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await puppeteer.launch({ executablePath: process.env.CHROME_BINARY || '/usr/bin/google-chrome', headless: true, args: ['--no-sandbox', '--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });
try {
  for (const [name, expected] of [['isolation', 'cross-origin isolation'], ['webgl', 'WebGL 2 is unavailable'], ['missing-wasm', 'wasm']]) {
    scenario = name;
    const page = await browser.newPage();
    if (name === 'webgl') await page.evaluateOnNewDocument(() => { const get = HTMLCanvasElement.prototype.getContext; HTMLCanvasElement.prototype.getContext = function(type, ...args) { return type === 'webgl2' ? null : get.call(this, type, ...args); }; });
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.waitForFunction(() => document.querySelector('#status').textContent === 'The editor could not start.', { timeout: 30000 });
    assert.equal(await page.$eval('#controls', el => el.disabled), true);
    assert.ok((await page.$eval('#loading', el => el.textContent)).toLowerCase().includes(expected.toLowerCase()));
    console.log(`PASS: visible startup failure for ${name}`);
    await page.close();
  }
} finally { await browser.close(); await new Promise(resolve => server.close(resolve)); }
