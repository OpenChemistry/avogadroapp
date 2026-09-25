// SPDX-License-Identifier: BSD-3-Clause
// The wrapper owns no C++ pointers; all messages are plain values.
export class AvogadroEditor extends EventTarget {
  constructor(instance) {
    super();
    this.instance = instance;
    instance.onEditorStateChanged = text => {
      this.dispatchEvent(new CustomEvent('statechange', { detail: JSON.parse(text) }));
    };
  }
  get state() { return JSON.parse(this.instance.editorState()); }
  setTool(name) { if (!this.instance.setTool(name)) throw new Error('Unknown tool.'); }
  setDrawOptions(options) {
    if (!this.instance.setDrawOptions(JSON.stringify(options))) throw new Error('Invalid draw options.');
  }
  newMolecule() { this.instance.newMolecule(); }
  loadMolecule(text, format) { return this.check(JSON.parse(this.instance.loadMolecule(text, format))); }
  exportMolecule(format = 'cjson') { return this.check(JSON.parse(this.instance.exportMolecule(format))).text; }
  check(result) { if (!result.ok) throw new Error(result.error || 'Unable to process this molecule.'); return result; }
  deleteSelection() { this.instance.deleteSelection(); }
  undo() { this.instance.undo(); }
  redo() { this.instance.redo(); }
  resetCamera() { this.instance.resetCamera(); }
  markSaved() { this.instance.markSaved(); }
}

const $ = id => document.getElementById(id);
const symbols = 'H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr Rf Db Sg Bh Hs Mt Ds Rg Cn Nh Fl Mc Lv Ts Og'.split(' ');
symbols.forEach((symbol, index) => $('element').add(new Option(`${symbol} (${index + 1})`, index + 1)));
$('element').value = '6';
let editor;
let filename = 'Untitled';
let busy = false;
const message = (text, error = false) => { $('status').textContent = text; $('status').dataset.error = String(error); };
function render(state) {
  $('document-name').textContent = `${filename}${state.modified ? ' *' : ''}`;
  $('counts').textContent = `${state.atomCount} atoms · ${state.bondCount} bonds`;
  $('undo').disabled = !state.canUndo;
  $('redo').disabled = !state.canRedo;
  document.querySelectorAll('[data-tool]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.tool === state.tool)));
  $('draw-options').hidden = state.tool !== 'draw';
  $('element').value = String(state.drawOptions.atomicNumber);
  $('bond').value = String(state.drawOptions.bondOrder);
  $('hydrogens').checked = state.drawOptions.adjustHydrogens;
}
async function confirmReplacement() {
  if (!editor.state.modified) return true;
  const dialog = $('discard-dialog');
  dialog.returnValue = 'cancel';
  const result = new Promise(resolve => dialog.addEventListener('close', () => resolve(dialog.returnValue === 'discard'), { once: true }));
  dialog.showModal();
  return result;
}
async function action(fn, focus = true) {
  if (busy || !editor) return;
  busy = true;
  try { await fn(); render(editor.state); if (focus) $('viewport').focus(); }
  catch (error) { message(error.message, true); }
  finally { busy = false; }
}

$('new').onclick = () => action(async () => {
  if (await confirmReplacement()) { editor.newMolecule(); filename = 'Untitled'; message(''); }
});
$('open').onclick = () => $('file').click();
$('file').onchange = () => action(async () => {
  const file = $('file').files[0];
  $('file').value = '';
  if (!file) return;
  // Read first, then confirm against the current document. Editing may have
  // continued while the browser was reading the file.
  const text = await file.text();
  if (!(await confirmReplacement())) return;
  const result = editor.loadMolecule(text, file.name.split('.').pop().toLowerCase());
  filename = file.name;
  message(result.notice || 'Molecule opened.');
});
$('download').onclick = () => action(() => {
  const format = $('format').value;
  const text = editor.exportMolecule(format);
  const link = document.createElement('a');
  const url = URL.createObjectURL(new Blob([text], { type: 'text/plain;charset=utf-8' }));
  link.href = url;
  link.download = `${filename.replace(/\.[^.]+$/, '')}.${format}`;
  document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  // Other exports may lose properties, so only CJSON clears the unsaved marker.
  if (format === 'cjson') editor.markSaved();
  message(format === 'cjson' ? 'Download requested.' : 'Export requested. Use CJSON to preserve Avogadro data.');
});
document.querySelectorAll('[data-tool]').forEach(button => { button.onclick = () => action(() => editor.setTool(button.dataset.tool)); });
for (const id of ['element', 'bond', 'hydrogens']) $(id).onchange = () => action(() => editor.setDrawOptions({ atomicNumber: Number($('element').value), bondOrder: Number($('bond').value), adjustHydrogens: $('hydrogens').checked }), false);
for (const [id, method] of [['undo', 'undo'], ['redo', 'redo'], ['delete', 'deleteSelection'], ['reset', 'resetCamera']]) $(id).onclick = () => action(() => editor[method]());
$('viewport').addEventListener('contextmenu', event => event.preventDefault());
$('viewport').addEventListener('keydown', event => {
  if (busy || !editor || $('discard-dialog').open) return;
  if ((event.ctrlKey || event.metaKey) && ['z', 'y'].includes(event.key.toLowerCase())) {
    event.preventDefault(); event.stopPropagation();
    action(() => event.key.toLowerCase() === 'y' || event.shiftKey ? editor.redo() : editor.undo());
  } else if (event.key === 'Delete' || event.key === 'Backspace') {
    event.preventDefault(); event.stopPropagation(); action(() => editor.deleteSelection());
  }
}, true);
window.addEventListener('beforeunload', event => {
  if (editor?.state.modified) { event.preventDefault(); event.returnValue = ''; }
});

try {
  if (!globalThis.WebAssembly) throw new Error('This browser does not support WebAssembly.');
  if (!globalThis.crossOriginIsolated || !globalThis.SharedArrayBuffer) throw new Error('This build needs cross-origin isolation. Serve it with serve-wasm.py or configure COOP: same-origin and COEP: require-corp.');
  const probe = document.createElement('canvas').getContext('webgl2');
  if (!probe) throw new Error('WebGL 2 is unavailable. Enable browser hardware acceleration or use a supported device.');
  probe.getExtension('WEBGL_lose_context')?.loseContext();
  const instance = await qtLoad({
    qt: { containerElements: [$('viewport')], entryFunction: globalThis.createAvogadroWeb,
      onExit: details => { $('controls').disabled = true; message(`Avogadro stopped: ${details.text || details.code || 'runtime exited'}`, true); } },
    print: text => console.info(text), printErr: text => console.error(text),
  });
  editor = new AvogadroEditor(instance);
  editor.addEventListener('statechange', event => render(event.detail));
  new ResizeObserver(() => instance.qtResizeContainerElement($('viewport'))).observe($('viewport'));
  $('loading').hidden = true; $('controls').disabled = false;
  render(editor.state);
} catch (error) {
  console.error(error);
  $('loading').textContent = error.message;
  message('The editor could not start.', true);
}
