import * as THREE from 'three';
import { OrbitControls } from './vendor/OrbitControls.js';

/* ---------- small helpers ---------- */
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const fmt = (v, d = 4) => (Number.isFinite(v) ? (Math.abs(v) < 1e-3 && v !== 0 ? v.toExponential(2) : +v.toFixed(d)).toString() : '–');

function showError(msg) { const b = $('#banner'); b.textContent = msg; b.hidden = false; }
function clearError() { $('#banner').hidden = true; }

async function api(path, opts = {}) {
  let res;
  try { res = await fetch(path, opts); }
  catch (e) { throw new Error('Cannot reach the local workbench server. If you closed Launch.exe, start it again.'); }
  let body = null;
  const text = await res.text();
  try { body = JSON.parse(text); } catch { /* non-JSON (csv) */ }
  if (!res.ok) throw new Error(body && body.error ? body.error : `Server error ${res.status}`);
  return body ?? text;
}
const post = (path, obj) => api(path, { method: 'POST', body: JSON.stringify(obj ?? {}) });

function download(name, text, type = 'text/csv') {
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([text], { type }));
  a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}

/* ---------- form validation (client side; the server validates again) ---------- */
function validateForm(form, rules) {
  const msgs = [];
  $$('input', form).forEach(i => i.classList.remove('invalid'));
  for (const [name, r] of Object.entries(rules)) {
    const el = form.querySelector(`[name="${name}"]`);
    const v = el.value.trim() === '' ? NaN : Number(el.value);
    let m = '';
    if (!Number.isFinite(v)) m = `${r.label} must be a number.`;
    else if (r.int && !Number.isInteger(v)) m = `${r.label} must be a whole number.`;
    else if (r.gt !== undefined && !(v > r.gt)) m = `${r.label} must be greater than ${r.gt}.`;
    else if (r.min !== undefined && v < r.min) m = `${r.label} must be at least ${r.min}.`;
    else if (r.max !== undefined && v > r.max) m = `${r.label} must be at most ${r.max}.`;
    if (m) { el.classList.add('invalid'); msgs.push(m); }
  }
  let box = $('.errmsg', form);
  if (!box) { box = document.createElement('div'); box.className = 'errmsg'; form.appendChild(box); }
  box.textContent = msgs.join(' ');
  return msgs.length === 0;
}
const feaRules = {
  L: { label: 'Length L', gt: 0 }, A: { label: 'Area A', gt: 0 }, k: { label: 'Conductivity k', gt: 0 },
  q: { label: 'Heat generation q' }, T0: { label: 'T(0)' }, TL: { label: 'T(L)' },
  N: { label: 'Number of elements N', int: true, min: 1, max: 1000000 },
};
const perfRules = {
  cases: { label: 'Cases', int: true, min: 1, max: 1000000 }, elements: { label: 'Elements per case', int: true, min: 2, max: 100000 },
  seed: { label: 'Seed', int: true, min: 0 }, maxWorkers: { label: 'Max workers', int: true, min: 1, max: 64 },
};
const formValues = f => Object.fromEntries([...new FormData(f)].map(([k, v]) => [k, Number(v)]));

/* ---------- tabs ---------- */
$$('#tabs button').forEach(b => b.addEventListener('click', () => {
  $$('#tabs button').forEach(x => x.classList.toggle('active', x === b));
  $$('.view').forEach(v => v.hidden = v.id !== 'view-' + b.dataset.view);
  clearError();
  if (b.dataset.view === 'hardware' && !$('#devCards').children.length) discover();
  if (b.dataset.view === 'learning' && !bpLoaded) loadBackprop();
  window.dispatchEvent(new Event('resize'));
}));
$$('.subtabs button').forEach(b => b.addEventListener('click', () => {
  $$('.subtabs button').forEach(x => x.classList.toggle('active', x === b));
  $('#sub-stl').hidden = b.dataset.sub !== 'stl';
  $('#sub-csv').hidden = b.dataset.sub !== 'csv';
  window.dispatchEvent(new Event('resize'));
}));

/* ---------- heartbeat so Launch.exe exits when the tab closes ---------- */
setInterval(() => fetch('/api/ping').catch(() => {}), 2000);
fetch('/api/ping').catch(() => {});
$('#quit').addEventListener('click', async () => {
  if (!confirm('Stop the workbench and close this page?')) return;
  try { await fetch('/api/shutdown', { method: 'POST' }); } catch {}
  document.body.innerHTML = '<p style="padding:40px;font:16px system-ui">The workbench has stopped. You can close this tab.</p>';
});
api('/api/info').then(i => { $('#foot').textContent = `v${i.version} · local only (127.0.0.1) · ${i.system.logicalCores} logical CPUs · startup: server ready ${i.launch.serverReadyMs} ms, UI loaded ${i.launch.uiLoadedMs >= 0 ? i.launch.uiLoadedMs + ' ms' : '…'} (reported separately from compute timings) · file limit ${i.limits.maxFileMB} MB`; }).catch(e => showError(e.message));

/* =====================================================================
   3D viewer (Three.js): rotate / pan / zoom / fit
   ===================================================================== */
function makeViewer(container) {
  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  container.prepend(renderer.domElement);
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x0f1722);
  const camera = new THREE.PerspectiveCamera(45, 1, 0.001, 1000);
  camera.up.set(0, 0, 1); // CAD convention: Z up
  camera.position.set(2, 2, 2);
  const controls = new OrbitControls(camera, renderer.domElement);
  controls.screenSpacePanning = true;
  controls.enableDamping = false;
  scene.add(new THREE.HemisphereLight(0xffffff, 0x334455, 0.9));
  const dl = new THREE.DirectionalLight(0xffffff, 1.2); dl.position.set(3, 2, 4); scene.add(dl);
  const v = { renderer, scene, camera, controls, content: null, sphere: null };
  const render = () => renderer.render(scene, camera);
  v.render = render;
  controls.addEventListener('change', render);
  const resize = () => {
    const w = container.clientWidth, h = container.clientHeight;
    if (!w || !h) return;
    renderer.setSize(w, h, false);
    camera.aspect = w / h; camera.updateProjectionMatrix(); render();
  };
  new ResizeObserver(resize).observe(container);
  v.setContent = (obj, sphere) => {
    if (v.content) { scene.remove(v.content); v.content.traverse(o => { o.geometry?.dispose(); o.material?.dispose?.(); }); }
    v.content = obj; v.sphere = sphere; scene.add(obj); v.fit();
  };
  v.fit = () => {
    if (!v.sphere) return;
    const { center, radius } = v.sphere;
    const r = Math.max(radius, 1e-9);
    const dist = (r / Math.sin(THREE.MathUtils.degToRad(camera.fov / 2))) * 1.15;
    const dir = new THREE.Vector3(1, -1.2, 0.9).normalize();
    camera.position.copy(center).addScaledVector(dir, dist);
    camera.near = dist / 1000; camera.far = dist * 100; camera.updateProjectionMatrix();
    controls.target.copy(center); controls.update(); render();
  };
  return v;
}

/* ---------- STL viewer ---------- */
const stlV = makeViewer($('#stlViewer'));
let stlMesh = null;
function showStl(j) {
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(j.positions), 3));
  geo.computeVertexNormals(); geo.computeBoundingSphere();
  const mat = new THREE.MeshStandardMaterial({ color: 0x8fb8e8, side: THREE.DoubleSide, flatShading: true, metalness: 0.1, roughness: 0.6, wireframe: $('#stlWire').checked });
  stlMesh = new THREE.Mesh(geo, mat);
  stlV.setContent(stlMesh, { center: geo.boundingSphere.center.clone(), radius: geo.boundingSphere.radius });
  const d = j.boundsMax.map((m, i) => m - j.boundsMin[i]);
  $('#stlInfo').textContent = `${j.triangles.toLocaleString()} triangles · size ${d.map(x => fmt(x, 4)).join(' × ')} model units` + (j.source ? ` · ${j.source}` : '');
}
$('#stlWire').addEventListener('change', e => { if (stlMesh) { stlMesh.material.wireframe = e.target.checked; stlV.render(); } });
$('#stlFit').addEventListener('click', () => stlV.fit());
$('#stlExample').addEventListener('click', async () => { clearError(); try { showStl(await api('/api/examples/stl')); } catch (e) { showError(e.message); } });
$('#stlFile').addEventListener('change', async e => {
  const f = e.target.files[0]; if (!f) return; clearError();
  $('#stlInfo').textContent = `Reading ${f.name}…`;
  try { showStl(await api('/api/geometry/stl', { method: 'POST', body: await f.text() })); }
  catch (err) { showError(`${f.name}: ${err.message}`); $('#stlInfo').textContent = 'Load failed.'; }
  e.target.value = '';
});

/* ---------- nodal CSV point cloud ---------- */
const csvV = makeViewer($('#csvViewer'));
const STOPS = [[0, [0.19, 0.21, 0.58]], [0.25, [0.27, 0.46, 0.71]], [0.5, [0.45, 0.8, 0.35]], [0.75, [0.99, 0.85, 0.25]], [1, [0.84, 0.19, 0.15]]];
function colorAt(t) {
  t = Math.min(1, Math.max(0, t));
  for (let i = 1; i < STOPS.length; i++) if (t <= STOPS[i][0]) {
    const [t0, c0] = STOPS[i - 1], [t1, c1] = STOPS[i], u = (t - t0) / (t1 - t0);
    return c0.map((c, k) => c + (c1[k] - c) * u);
  }
  return STOPS[STOPS.length - 1][1];
}
let cloud = null, pts = null, marker = null;
function drawLegend(minT, maxT) {
  const c = $('#legendCanvas'), g = c.getContext('2d');
  for (let y = 0; y < c.height; y++) { const [r, gg, b] = colorAt(1 - y / (c.height - 1)); g.fillStyle = `rgb(${r * 255 | 0},${gg * 255 | 0},${b * 255 | 0})`; g.fillRect(0, y, c.width, 1); }
  $('#legMax').textContent = fmt(maxT, 2); $('#legMid').textContent = fmt((minT + maxT) / 2, 2); $('#legMin').textContent = fmt(minT, 2);
  $('#legend').hidden = false;
}
function showCloud(j) {
  cloud = j;
  const n = j.count, pos = new Float32Array(n * 3), col = new Float32Array(n * 3);
  const span = j.maxT - j.minT;
  for (let i = 0; i < n; i++) {
    pos.set([j.x[i], j.y[i], j.z[i]], i * 3);
    col.set(colorAt(span > 0 ? (j.T[i] - j.minT) / span : 0.5), i * 3);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.computeBoundingSphere();
  pts = new THREE.Points(geo, new THREE.PointsMaterial({ size: 8, sizeAttenuation: false, vertexColors: true }));
  const g = new THREE.Group(); g.add(pts);
  const mg = new THREE.BufferGeometry(); mg.setAttribute('position', new THREE.BufferAttribute(new Float32Array(3), 3));
  marker = new THREE.Points(mg, new THREE.PointsMaterial({ size: 16, sizeAttenuation: false, color: 0xffffff, depthTest: false }));
  marker.visible = false; marker.renderOrder = 10; g.add(marker);
  csvV.setContent(g, { center: geo.boundingSphere.center.clone(), radius: geo.boundingSphere.radius });
  drawLegend(j.minT, j.maxT);
  $('#inspect').hidden = true;
  $('#csvInfo').textContent = `${n.toLocaleString()} nodes · T ${fmt(j.minT, 2)} to ${fmt(j.maxT, 2)} °C · x ${fmt(j.boundsMin[0])}…${fmt(j.boundsMax[0])} m` + (j.source ? ` · ${j.source}` : '');
}
$('#csvFit').addEventListener('click', () => csvV.fit());
$('#csvExample').addEventListener('click', async () => { clearError(); try { showCloud(await api('/api/examples/csv')); } catch (e) { showError(e.message); } });
$('#csvFile').addEventListener('change', async e => {
  const f = e.target.files[0]; if (!f) return; clearError();
  $('#csvInfo').textContent = `Reading ${f.name}…`;
  try { showCloud(await api('/api/results/csv', { method: 'POST', body: await f.text() })); }
  catch (err) { showError(`${f.name}: ${err.message}`); $('#csvInfo').textContent = 'Load failed.'; }
  e.target.value = '';
});
$('#csvPng').addEventListener('click', () => {
  if (!cloud) return showError('Load a nodal CSV first.');
  csvV.render();
  const a = document.createElement('a'); a.href = csvV.renderer.domElement.toDataURL('image/png'); a.download = 'nodal_temperature_view.png'; a.click();
});
// node inspection: click the nearest node within ~8 px
const ray = new THREE.Raycaster();
let downAt = null;
csvV.renderer.domElement.addEventListener('pointerdown', e => { downAt = [e.clientX, e.clientY]; });
csvV.renderer.domElement.addEventListener('pointerup', e => {
  if (!cloud || !downAt || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;   // a drag, not a click
  const r = csvV.renderer.domElement.getBoundingClientRect();
  const ndc = new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  const cam = csvV.camera, dist = cam.position.distanceTo(csvV.controls.target);
  const worldPerPx = (2 * dist * Math.tan(THREE.MathUtils.degToRad(cam.fov / 2))) / r.height;
  ray.params.Points.threshold = worldPerPx * 8;
  ray.setFromCamera(ndc, cam);
  const hits = ray.intersectObject(pts);
  if (!hits.length) { marker.visible = false; $('#inspect').hidden = true; csvV.render(); return; }
  hits.sort((a, b) => a.distanceToRay - b.distanceToRay);
  const i = hits[0].index;
  marker.geometry.attributes.position.set([cloud.x[i], cloud.y[i], cloud.z[i]]); marker.geometry.attributes.position.needsUpdate = true;
  marker.visible = true;
  const box = $('#inspect');
  box.innerHTML = `<b>Node ${cloud.ids[i]}</b><br>x = ${fmt(cloud.x[i], 6)} m<br>y = ${fmt(cloud.y[i], 6)} m<br>z = ${fmt(cloud.z[i], 6)} m<br>T = ${fmt(cloud.T[i], 3)} °C`;
  box.hidden = false; csvV.render();
});

/* =====================================================================
   Thermal analysis (Task C)
   ===================================================================== */
const COLORS = ['#d62728', '#2ca02c', '#9467bd', '#ff7f0e'];
let feaChart = null, convChart = null, lastFea = null;
const feaForm = $('#feaForm');
const axisTitle = t => ({ display: true, text: t });
function feaParams() { return formValues(feaForm); }

feaForm.addEventListener('submit', async e => {
  e.preventDefault(); clearError();
  if (!validateForm(feaForm, feaRules)) return;
  const btn = $('button[type=submit]', feaForm); btn.disabled = true;
  try {
    const p = feaParams(), r = await post('/api/fea/solve', p);
    lastFea = r;
    const pts = (xs, ys) => xs.map((x, i) => ({ x, y: ys[i] }));
    const dense = Array.from({ length: 201 }, (_, i) => p.L * i / 200);
    const exact = dense.map(x => p.T0 + (p.TL - p.T0) * x / p.L + (p.q / (2 * p.k)) * x * (p.L - x));
    feaChart?.destroy();
    feaChart = new Chart($('#feaChart'), { type: 'scatter', data: { datasets: [
      { label: 'Exact', data: pts(dense, exact), showLine: true, pointRadius: 0, borderColor: '#1f77b4', borderWidth: 2 },
      { label: `FE (N=${r.N})`, data: pts(r.x, r.T), showLine: true, pointRadius: r.N <= 40 ? 3 : 0, borderColor: '#d62728', backgroundColor: '#d62728', borderWidth: 1.5 }] },
      options: { maintainAspectRatio: false, animation: false, scales: { x: { title: axisTitle('x [m]') }, y: { title: axisTitle('Temperature T [°C]') } }, plugins: { title: { display: true, text: 'Rod temperature profile' } } } });
    $('#feaStats').innerHTML = `Element size h = <b>${fmt(r.h, 6)}</b> m<br>Max nodal error = <b>${fmt(r.maxNodalError, 3)}</b> °C<br>Max midpoint error (FE interpolation) = <b>${fmt(r.maxMidpointError, 6)}</b> °C`;
  } catch (err) { showError(err.message); } finally { btn.disabled = false; }
});
$('#feaConv').addEventListener('click', async () => {
  clearError();
  if (!validateForm(feaForm, feaRules)) return;
  try {
    const r = await post('/api/fea/convergence', feaParams());
    const tb = $('#convTable tbody'); tb.innerHTML = '';
    r.rows.forEach(w => tb.insertAdjacentHTML('beforeend', `<tr><td>${w.N}</td><td>${fmt(w.h, 5)}</td><td>${w.maxNodalError.toExponential(2)}</td><td>${fmt(w.maxMidpointError, 6)}</td><td>${w.observedOrder ? w.observedOrder.toFixed(3) : '–'}</td></tr>`));
    $('#convBox').hidden = false;
    const pts = (xs, ys) => xs.map((x, i) => ({ x, y: ys[i] }));
    feaChart?.destroy();
    feaChart = new Chart($('#feaChart'), { type: 'scatter', data: { datasets: [
      { label: 'Exact', data: pts(r.exact.x, r.exact.T), showLine: true, pointRadius: 0, borderColor: '#1f77b4', borderWidth: 2 },
      ...r.solutions.map((s, i) => ({ label: `FE N=${s.N}`, data: pts(s.x, s.T), showLine: true, pointRadius: s.N <= 10 ? 3 : 0, borderColor: COLORS[i], backgroundColor: COLORS[i], borderWidth: 1.2 }))] },
      options: { maintainAspectRatio: false, animation: false, scales: { x: { title: axisTitle('x [m]') }, y: { title: axisTitle('Temperature T [°C]') } }, plugins: { title: { display: true, text: 'FE solutions for N = 10, 20, 40, 80' } } } });
    convChart?.destroy();
    convChart = new Chart($('#convChart'), { type: 'scatter', data: { datasets: [{ label: 'Max midpoint error', data: r.rows.map(w => ({ x: w.h, y: w.maxMidpointError })), showLine: true, borderColor: '#d62728', backgroundColor: '#d62728', pointRadius: 4 }] },
      options: { maintainAspectRatio: false, animation: false, scales: { x: { type: 'logarithmic', title: axisTitle('Element size h [m]') }, y: { type: 'logarithmic', title: axisTitle('Max midpoint error [°C]') } }, plugins: { title: { display: true, text: 'Convergence (slope 2 on log–log axes)' } } } });
  } catch (err) { showError(err.message); }
});
async function feaExport(kind, name) {
  clearError();
  if (!validateForm(feaForm, feaRules)) return;
  try { download(name, await api('/api/fea/export', { method: 'POST', body: JSON.stringify({ ...feaParams(), kind }) })); } catch (err) { showError(err.message); }
}
$('#feaCsv').addEventListener('click', () => feaExport('solution', 'fea_solution.csv'));
$('#feaConvCsv').addEventListener('click', () => feaExport('convergence', 'fea_convergence.csv'));
$('#feaPng').addEventListener('click', () => {
  if (!feaChart) return showError('Solve first, then export the plot.');
  const a = document.createElement('a'); a.href = feaChart.toBase64Image(); a.download = 'fea_plot.png'; a.click();
});

/* =====================================================================
   Performance (Task D)
   ===================================================================== */
const perfForm = $('#perfForm');
let perfTimer = null, perfChart = null, perfCsvText = '';
function setPerfBusy(b) {
  $('#perfStart').disabled = b; $('#perfCancel').disabled = !b; $('#perfProgress').hidden = !b;
}
perfForm.addEventListener('submit', async e => {
  e.preventDefault(); clearError();
  if (!validateForm(perfForm, perfRules)) return;
  try {
    await post('/api/perf/start', formValues(perfForm));
    setPerfBusy(true); $('#perfCsv').disabled = true; $('#perfResult').hidden = true;
    clearInterval(perfTimer);
    perfTimer = setInterval(pollPerf, 300);
  } catch (err) { showError(err.message); }
});
$('#perfCancel').addEventListener('click', async () => { try { await post('/api/perf/cancel'); $('#perfLabel').textContent = 'Cancelling after the current run…'; } catch (e) { showError(e.message); } });
async function pollPerf() {
  try {
    const s = await api('/api/perf/status');
    $('#perfProgress').value = s.progress; $('#perfLabel').textContent = s.state === 'running' ? `Running: ${s.label} (${Math.round(s.progress * 100)}%)` : s.label;
    if (s.state === 'running') return;
    clearInterval(perfTimer); setPerfBusy(false);
    if (s.state === 'error') return showError('Benchmark failed: ' + s.error);
    if (s.state === 'cancelled') { $('#perfLabel').textContent = 'Cancelled. Partial results were discarded.'; return; }
    if (s.state === 'done') renderPerf(s.result);
  } catch (err) { clearInterval(perfTimer); setPerfBusy(false); showError(err.message); }
}
function renderPerf(r) {
  perfCsvText = r.csv; $('#perfCsv').disabled = false; $('#perfLabel').textContent = 'Done.';
  const s = r.system;
  $('#perfSys').textContent = `${s.cpu} · ${s.physicalCores || '?'} physical / ${s.logicalCores} logical cores · ${s.ramGiB.toFixed(1)} GiB RAM · ${s.os} · ${s.compiler} · flags: ${s.flags} · workload ${r.cases} cases × ${r.elements} elements, seed ${r.seed}`;
  const tb = $('#perfTable tbody'); tb.innerHTML = '';
  r.rows.forEach(w => tb.insertAdjacentHTML('beforeend', `<tr><td>${w.workers}${w.workers === 1 ? ' (serial)' : ''}</td><td>${w.medianSeconds.toFixed(4)}</td><td>${w.speedup.toFixed(2)}</td><td>${w.efficiency.toFixed(2)}</td><td>${w.maxDiff.toExponential(1)}</td></tr>`));
  $('#perfVerdict').innerHTML = `<p>Serial/parallel agreement (≤ 1e-8 °C): <span class="${r.agreementOk ? 'ok' : 'bad'}">${r.agreementOk ? 'PASS' : 'FAIL'}</span></p>` + (r.note ? `<p class="bad">${r.note}</p>` : '');
  $('#perfResult').hidden = false;
  perfChart?.destroy();
  perfChart = new Chart($('#perfChart'), { data: { labels: r.rows.map(w => w.workers + (w.workers === 1 ? ' (serial)' : '')), datasets: [
    { type: 'bar', label: 'Measured speedup T₁/Tₚ', data: r.rows.map(w => w.speedup), backgroundColor: '#1f77b4' },
    { type: 'line', label: 'Ideal', data: r.rows.map(w => w.workers), borderColor: '#999', borderDash: [6, 4], pointRadius: 0 }] },
    options: { maintainAspectRatio: false, animation: false, scales: { x: { title: axisTitle('Workers') }, y: { title: axisTitle('Speedup (×)'), beginAtZero: true } }, plugins: { title: { display: true, text: 'Parallel speedup (median of 5 runs)' } } } });
}
$('#perfCsv').addEventListener('click', () => download('benchmark.csv', perfCsvText));

/* =====================================================================
   Hardware (Task E) — simulated
   ===================================================================== */
const log = msg => { const l = $('#devLog'); l.textContent += `${new Date().toLocaleTimeString()}  [SIMULATED] ${msg}\n`; l.scrollTop = l.scrollHeight; };
async function discover() {
  clearError();
  try { const r = await api('/api/devices'); renderDevices(r.devices); log(`Discovery found ${r.devices.length} devices.`); } catch (e) { showError(e.message); }
}
function renderDevices(devs) {
  const box = $('#devCards'); box.innerHTML = '';
  devs.forEach(d => {
    const c = document.createElement('div'); c.className = 'card dev'; c.id = 'dev-' + d.id;
    const on = d.state === 'connected';
    c.innerHTML = `<div><span class="sim-tag">SIMULATED</span> <span class="badge ${on ? 'on' : ''}">${d.state}</span></div>
      <h2>${d.name}</h2><div class="muted">${d.role}</div>
      <ul>${d.capabilities.map(x => `<li>${x}</li>`).join('')}</ul>
      <div class="actions">
        <button data-a="${on ? 'disconnect' : 'connect'}">${on ? 'Disconnect' : 'Connect'}</button>
        <button data-a="telemetry" ${on ? '' : 'disabled'}>Read telemetry</button>
        <button data-a="inject-timeout">Simulate timeout</button>
        <button data-a="connect-retry" ${on ? 'disabled' : ''}>Connect with retry</button>
      </div><div class="tele"></div>`;
    $$('button', c).forEach(b => b.addEventListener('click', () => devAction(d.id, b.dataset.a)));
    box.appendChild(c);
  });
}
async function devAction(id, action) {
  clearError();
  try {
    const r = await post(`/api/devices/${id}/${action}`);
    const st = r.status;
    if (st.ok) log(`${id} ${action}: OK${st.attempts > 1 ? ` (recovered after ${st.attempts} attempts)` : ''}${st.message ? ' - ' + st.message : ''}`);
    else log(`${id} ${action}: FAILED (${st.error}) - ${st.message}`);
    const list = (await api('/api/devices')).devices;
    renderDevices(list);
    const tele = $(`#dev-${id} .tele`);
    if (r.telemetry) tele.innerHTML = `#${r.telemetry.sequence} ` + r.telemetry.values.map(v => `${v.name}=${fmt(v.value, 3)} ${v.unit}`).join(' · ');
    else if (!st.ok) tele.innerHTML = `<span class="bad">${st.message}</span>`;
  } catch (e) { showError(e.message); log(`${id} ${action}: ${e.message}`); }
}
$('#devDiscover').addEventListener('click', discover);

/* =====================================================================
   Backprop (Task F)
   ===================================================================== */
let bpLoaded = false, bpCsv = '';
async function loadBackprop() {
  try {
    const r = await api('/api/backprop'); bpLoaded = true; bpCsv = r.csv;
    const rows = [['z = wx + b', r.forward.z], ['h = tanh(z)', r.forward.h], ['ŷ = vh + c', r.forward.yhat], ['J = ½(ŷ − y)²', r.forward.J],
      ['dJ/dŷ', r.chain.dJ_dyhat], ['dJ/dh', r.chain.dJ_dh], ['dh/dz = 1 − h²', r.chain.dh_dz], ['dJ/dz', r.chain.dJ_dz],
      ['dJ/dw', r.grad.w], ['dJ/db', r.grad.b], ['dJ/dv', r.grad.v], ['dJ/dc', r.grad.c],
      ['w₁', r.updated.w], ['b₁', r.updated.b], ['v₁', r.updated.v], ['c₁', r.updated.c], ['J after one update', r.lossAfterUpdate]];
    const tb = $('#bpTable tbody'); tb.innerHTML = '';
    rows.forEach(([k, v]) => tb.insertAdjacentHTML('beforeend', `<tr><td>${k}</td><td>${v.toFixed(10)}</td></tr>`));
    $('#bpCheck').innerHTML = `<p>Central-difference check (ε = 1e-6): max error <b>${r.maxGradError.toExponential(2)}</b> → <span class="${r.gradCheckPass ? 'ok' : 'bad'}">${r.gradCheckPass ? 'PASS (≤ 1e-5)' : 'FAIL'}</span></p>`;
    new Chart($('#bpChart'), { type: 'scatter', data: { datasets: [{ label: 'Loss J', data: r.loss.map((y, x) => ({ x, y: Math.max(y, 1e-18) })), showLine: true, pointRadius: 0, borderColor: '#d62728' }] },
      options: { maintainAspectRatio: false, animation: false, scales: { x: { title: axisTitle('Training step') }, y: { type: 'logarithmic', title: axisTitle('Loss J') } }, plugins: { title: { display: true, text: 'Loss over 100 gradient-descent steps (lr = 0.1)' } } } });
  } catch (e) { showError(e.message); }
}
$('#bpCsv').addEventListener('click', () => download('backprop_loss.csv', bpCsv));
