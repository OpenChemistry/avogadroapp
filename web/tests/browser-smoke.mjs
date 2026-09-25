// SPDX-License-Identifier: BSD-3-Clause
// Run: node browser-smoke.mjs URL EVIDENCE_DIRECTORY
// Owns a new headless Chrome process/profile; never connects to a user's browser.
import { spawn } from 'node:child_process';
import { mkdtemp, mkdir, writeFile, readFile, readdir, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import assert from 'node:assert/strict';

const url = process.argv[2] || 'http://127.0.0.1:8000';
const output = resolve(process.argv[3] || 'browser-evidence');
await mkdir(output, { recursive: true });
const profile = await mkdtemp(join(tmpdir(), 'avogadro-browser-'));
const browser = spawn(process.env.CHROME_BINARY || 'google-chrome', [
  '--headless', '--no-sandbox', '--disable-dev-shm-usage', '--remote-debugging-pipe',
  '--disable-background-networking', '--disable-sync', '--no-first-run',
  `--user-data-dir=${profile}`, '--enable-unsafe-swiftshader', '--use-angle=swiftshader',
  'about:blank',
], { stdio: ['ignore', 'ignore', 'pipe', 'pipe', 'pipe'] });
let sequence = 0, buffer = '', session;
const pending = new Map(), errors = [], checks = [];
browser.stderr.on('data', data => process.stderr.write(data));
browser.stdio[4].on('data', data => {
  buffer += data.toString();
  while (buffer.includes('\0')) {
    const index = buffer.indexOf('\0');
    const message = JSON.parse(buffer.slice(0, index)); buffer = buffer.slice(index + 1);
    if (message.id) {
      const request = pending.get(message.id); pending.delete(message.id);
      if (message.error) request?.reject(new Error(JSON.stringify(message.error)));
      else request?.resolve(message.result);
    } else if (message.method === 'Runtime.exceptionThrown') {
      errors.push(message.params.exceptionDetails.exception?.description || message.params.exceptionDetails.text);
      console.error(errors.at(-1));
    } else if (message.method === 'Runtime.consoleAPICalled' && message.params.type === 'error') {
      errors.push(message.params.args.map(arg => arg.value || arg.description).join(' '));
      console.error(errors.at(-1));
    }
  }
});
function send(method, params = {}, target = session) {
  const id = ++sequence;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(id); reject(new Error(`CDP timeout: ${method}`)); }, 15000);
    pending.set(id, { resolve: value => { clearTimeout(timer); resolve(value); }, reject: error => { clearTimeout(timer); reject(error); } });
    browser.stdio[3].write(JSON.stringify({ id, method, params, ...(target ? { sessionId: target } : {}) }) + '\0');
  });
}
async function evaluate(expression) {
  const response = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (response.exceptionDetails) throw new Error(response.exceptionDetails.exception?.description || response.exceptionDetails.text);
  return response.result.value;
}
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
async function wait(expression, timeout = 90000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) { if (await evaluate(expression)) return; if (await evaluate(`document.querySelector('#status')?.textContent === 'The editor could not start.'`)) throw new Error(await evaluate(`document.querySelector('#loading').textContent`)); await delay(100); }
  throw new Error(`Timed out: ${expression}; ${await evaluate('document.body.innerText')}`);
}
async function click(id) { await evaluate(`document.getElementById(${JSON.stringify(id)}).click()`); await delay(150); }
async function set(id, value) {
  await evaluate(`{const el=document.getElementById(${JSON.stringify(id)});el.value=${JSON.stringify(value)};el.dispatchEvent(new Event('change',{bubbles:true}));}`);
  await delay(100);
}
async function load(path) {
  const { root } = await send('DOM.getDocument');
  const { nodeId } = await send('DOM.querySelector', { nodeId: root.nodeId, selector: '#file' });
  await send('DOM.setFileInputFiles', { nodeId, files: [path] });
  await delay(250);
}
async function screenshot(name) {
  const { data } = await send('Page.captureScreenshot', { format: 'png' });
  await writeFile(join(output, name), Buffer.from(data, 'base64'));
}
const watchdog = setTimeout(() => { browser.kill('SIGTERM'); process.exitCode = 1; }, 180000);
try {
  const { targetId } = await send('Target.createTarget', { url: 'about:blank' }, null);
  ({ sessionId: session } = await send('Target.attachToTarget', { targetId, flatten: true }, null));
  await send('Runtime.enable'); await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride', { width: 1200, height: 800, deviceScaleFactor: 1, mobile: false });
  await send('Browser.setDownloadBehavior', { behavior: 'allow', downloadPath: output }, null);
  console.log('Navigating', url);
  await send('Page.navigate', { url });
  await wait(`document.querySelector('#controls') && !document.querySelector('#controls').disabled`);
  console.log('WASM loaded');
  checks.push('WASM startup');
  assert.equal(await evaluate(`!!document.querySelector('#qt-shadow-container')?.shadowRoot?.querySelector('canvas')`), true);
  await delay(300);
  const rect = await evaluate(`(()=>{const r=document.querySelector('#viewport').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', ...rect, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...rect, button: 'left', clickCount: 1 });
  await wait(`document.querySelector('#counts').textContent.startsWith('5 atoms')`, 10000);
  checks.push('Draw carbon and automatic hydrogens');
  assert.equal(await evaluate(`(()=>{const event=new Event('beforeunload',{cancelable:true});window.dispatchEvent(event);return event.defaultPrevented;})()`), true);
  checks.push('Unsaved document navigation protection');
  await screenshot('draw.png');
  await click('undo'); await wait(`document.querySelector('#counts').textContent.startsWith('0 atoms')`, 5000);
  await click('redo'); await wait(`document.querySelector('#counts').textContent.startsWith('5 atoms')`, 5000);
  checks.push('Undo/redo through HTML controls');
  await click('new'); await wait(`document.querySelector('#discard-dialog').open`, 5000);
  await evaluate(`document.querySelector('#discard-dialog button[value="cancel"]').click()`);
  assert.equal(await evaluate(`document.querySelector('#counts').textContent.startsWith('5 atoms')`), true);
  await click('new'); await evaluate(`document.querySelector('#discard-dialog button[value="discard"]').click()`);
  await wait(`document.querySelector('#counts').textContent.startsWith('0 atoms')`, 5000);
  checks.push('Unsaved-change confirmation and cancellation');
  await set('element', '8'); await set('bond', '2');
  await click('hydrogens');
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', ...rect, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...rect, button: 'left', clickCount: 1 });
  await wait(`document.querySelector('#counts').textContent.startsWith('1 atoms')`, 5000);
  checks.push('HTML draw settings without Qt panels');
  // Selection deletion is one undo transaction; HTML focus must not edit.
  await evaluate(`document.querySelector('#element').focus()`);
  await send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8 });
  await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Backspace', code: 'Backspace', windowsVirtualKeyCode: 8 });
  await evaluate(`document.querySelector('[data-tool="select"]').click()`);
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', ...rect, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', ...rect, button: 'left', clickCount: 1 });
  await click('delete'); await wait(`document.querySelector('#counts').textContent.startsWith('0 atoms')`, 5000);
  await click('undo'); await wait(`document.querySelector('#counts').textContent.startsWith('1 atoms')`, 5000);
  checks.push('HTML keyboard focus, selection and grouped deletion undo');
  await click('download');
  await delay(400);
  const downloaded = (await readdir(output)).find(name => name.endsWith('.cjson'));
  assert.ok(downloaded, 'CJSON browser download');
  assert.deepEqual(JSON.parse(await readFile(join(output, downloaded), 'utf8')).atoms.elements.number, [8]);
  checks.push('CJSON download');
  const xyz = join(profile, 'hydrogen.xyz');
  await writeFile(xyz, '2\nHydrogen\nH 0 0 0\nH 0 0 0.74\n');
  await load(xyz);
  await wait(`document.querySelector('#counts').textContent.startsWith('2 atoms')`, 5000);
  const bad = join(profile, 'bad.cjson'); await writeFile(bad, 'not json'); await load(bad);
  await wait(`document.querySelector('#status').dataset.error === 'true'`, 5000);
  assert.equal(await evaluate(`document.querySelector('#counts').textContent.startsWith('2 atoms')`), true);
  checks.push('Browser file input; failed import preserves document');
  for (const format of ['mol', 'sdf', 'xyz']) { await set('format', format); await click('download'); }
  await delay(300);
  const files = await readdir(output);
  for (const extension of ['mol', 'sdf', 'xyz']) assert.ok(files.some(name => name.endsWith(`.${extension}`)));
  checks.push('MOL/SDF/XYZ downloads');
  await evaluate(`document.querySelector('[data-tool="navigate"]').click()`);
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', ...rect, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseMoved', x: rect.x + 70, y: rect.y + 30, buttons: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: rect.x + 70, y: rect.y + 30, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseWheel', ...rect, deltaX: 0, deltaY: 120 });
  await click('reset');
  await send('Emulation.setDeviceMetricsOverride', { width: 900, height: 700, deviceScaleFactor: 1, mobile: false });
  await delay(300); await screenshot('loaded.png');
  checks.push('Camera input, fit, viewport resize');
  assert.deepEqual(errors, []);
  await writeFile(join(output, 'result.json'), JSON.stringify({ checks, errors }, null, 2));
  console.log(JSON.stringify({ passed: checks.length, checks }));
} catch (error) {
  await screenshot('failure.png').catch(() => {});
  await writeFile(join(output, 'failure.json'), JSON.stringify({ error: String(error), errors, checks }, null, 2));
  console.error(error); process.exitCode = 1;
} finally {
  clearTimeout(watchdog);
  await send('Browser.close', {}, null).catch(() => {});
  await delay(300); browser.kill();
  await rm(profile, { recursive: true, force: true }).catch(() => {});
}
