// Builds docs/report.docx (justified, 14 pt body). Run from the repo root: node scripts/make_report.js
const fs = require('fs');
const { PageBreak, Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell, ImageRun, AlignmentType, WidthType,
  ShadingType, BorderStyle, HeadingLevel } = require('docx');

const FONT = 'Arial', BODY = 28;               // 14 pt
const W = 9906;                                 // content width (A4, 1000 DXA margins)
const LAUNCH_VM = process.env.LAUNCH_VM || 'server ready 6 ms and UI loaded 429 ms after process start, in a Windows 11 ARM virtual machine with 4 logical CPUs running the x64 build under emulation';
const CANDIDATE = process.env.CANDIDATE || 'RA2311056010232';

const run = (t, o = {}) => new TextRun({ text: t, font: FONT, size: BODY, ...o });
const P = (parts, o = {}) => new Paragraph({ alignment: AlignmentType.JUSTIFIED, spacing: { after: 100, line: 252 },
  children: (Array.isArray(parts) ? parts : [parts]).map(x => typeof x === 'string' ? run(x) : x), ...o });
const H = t => new Paragraph({ heading: HeadingLevel.HEADING_1, alignment: AlignmentType.JUSTIFIED, spacing: { before: 160, after: 80 },
  children: [new TextRun({ text: t, font: FONT, size: 32, bold: true })] });
const B = t => new Paragraph({ numbering: { reference: 'b', level: 0 }, alignment: AlignmentType.JUSTIFIED, spacing: { after: 40, line: 252 },
  children: (Array.isArray(t) ? t : [t]).map(x => typeof x === 'string' ? run(x) : x) });
const img = (f, w, h) => new ImageRun({ type: 'png', data: fs.readFileSync(f), transformation: { width: w, height: h },
  altText: { title: f, description: f, name: f } });
const border = { style: BorderStyle.SINGLE, size: 4, color: '999999' };
const borders = { top: border, bottom: border, left: border, right: border };
const none = { style: BorderStyle.NONE, size: 0, color: 'FFFFFF' };
const noBorders = { top: none, bottom: none, left: none, right: none };
function table(widths, rows, headerRow = true) {
  return new Table({ width: { size: widths.reduce((a, b) => a + b), type: WidthType.DXA }, columnWidths: widths,
    rows: rows.map((r, i) => new TableRow({ children: r.map((c, j) => new TableCell({
      width: { size: widths[j], type: WidthType.DXA }, borders,
      margins: { top: 30, bottom: 30, left: 70, right: 70 },
      shading: i === 0 && headerRow ? { fill: 'E8EEF7', type: ShadingType.CLEAR, color: 'auto' } : undefined,
      children: [new Paragraph({ alignment: AlignmentType.JUSTIFIED, children: [run(String(c), { bold: i === 0 && headerRow })] })] })) })) });
}
const sp = () => new Paragraph({ spacing: { after: 40 }, children: [] });

const sideBySide = new Table({ width: { size: W, type: WidthType.DXA }, columnWidths: [W / 2, W / 2], rows: [new TableRow({ children: [
  ['docs/img/fea_temperature.png', 'Fig. 1a: FE vs exact, N = 10 to 80.'],
  ['docs/img/fea_convergence.png', 'Fig. 1b: midpoint error vs h.'] ].map(([f, cap]) => new TableCell({
    width: { size: W / 2, type: WidthType.DXA }, borders: noBorders,
    children: [new Paragraph({ alignment: AlignmentType.CENTER, children: [img(f, 318, 192)] }),
               new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 40 }, children: [run(cap, { italics: true })] })] })) })] });

const doc = new Document({
  styles: { default: { document: { run: { font: FONT, size: BODY } } } },
  numbering: { config: [{ reference: 'b', levels: [{ level: 0, format: 'bullet', text: '•', alignment: AlignmentType.LEFT,
    style: { paragraph: { indent: { left: 400, hanging: 260 } } } }] }] },
  sections: [{ properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1000, bottom: 1000, left: 1000, right: 1000 } } },
    children: [
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [new TextRun({ text: 'Unified Engineering Workbench: Technical Report', font: FONT, size: 36, bold: true })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 100 }, children: [run('Candidate ID: ' + CANDIDATE)] }),

      H('1. Design choices'),
      P('The backend is C++17 and the front end is a browser page served from the same executable. Core logic (FEA, parsers, thread pool, batch, backprop, mock devices) lives in a static library, wb_core, with no HTTP or UI code. A thin server layer maps JSON requests onto it. Launch.exe starts the server on 127.0.0.1 only, on a free port, opens the default browser and exits when the page stops sending heartbeats, so no terminal, install or firewall prompt is needed. MSVC builds use the static runtime, and CI checks that Launch.exe imports no VC++ runtime DLL. Three.js and Chart.js are bundled locally, so the app works offline.'),
      P('Two inputs are supported. An ASCII STL is parsed and drawn as a rotatable, pannable, zoomable surface (units are model units because STL has no unit field). A nodal CSV with the header node_id,x_m,y_m,z_m,temperature_C (metres, degrees C) is drawn as a colour-mapped point cloud with a legend; clicking a node shows its id, coordinates and temperature. Malformed files are rejected with a line-numbered message and never crash the server. The bundled examples are synthetic and labelled as such in data/README.md.'),

      H('2. Thermal FEA verification'),
      P('Two-node linear elements assemble K = (kA/h)[[1,-1],[-1,1]] and a consistent load qAh/2 per node. Dirichlet values are moved to the right-hand side and the constrained rows become identity rows. The error is measured at element midpoints from the FE interpolation, not only at nodes.'),
      table([1100, 1700, 2800, 2800, 1506], [
        ['N', 'h [m]', 'Nodal error [C]', 'Midpoint error [C]', 'Order'],
        ['10', '0.1', '2.8e-14', '1.25', '-'], ['20', '0.05', '2.0e-13', '0.3125', '2.00'],
        ['40', '0.025', '8.8e-13', '0.0781', '2.00'], ['80', '0.0125', '1.2e-12', '0.0195', '2.00']]),
      sp(),
      P('The midpoint error equals (q/2k)h²/4 exactly, which a unit test checks. Debugging insight: nodal values are exact to round-off for a quadratic solution, so a nodes-only check would hide the discretisation error entirely; the h² behaviour only appears between nodes.'),

      sideBySide,

      H('3. Performance results'),
      P('Workload: 20,000 independent solves of 2,000 elements, seed 12345, results kept in input order. A bounded std::thread pool is created outside the timed region. Protocol: one warm-up, five measured runs, median reported; only the solve is timed. Machine: Apple M4 (10 cores, 10 logical), 16 GiB, macOS, Apple Clang 17, -O3 -DNDEBUG, a passively cooled MacBook Air.'),
      table([1300, 1900, 1900, 1900, 2906], [
        ['Workers', 'Run 1 [s]', 'Run 2 [s]', 'Speedup', 'Efficiency'],
        ['1', '0.2348', '0.2453', '1.00', '1.00'],
        ['2', '0.1242', '0.1234', '1.89 / 1.99', '0.95 / 0.99'],
        ['4', '0.0668', '0.0668', '3.51 / 3.67', '0.88 / 0.92']]),
      sp(),
      P('Serial and parallel temperatures agree exactly (maximum difference 0, limit 1e-8 C). The parallel medians repeat within 1% between runs, while the serial median moved by 4% (individual serial runs ranged from 0.228 to 0.275 s), so most of the speedup difference between runs comes from the serial baseline. Efficiency falls at 4 workers because of thread start-up and queueing, memory bandwidth, and a short run (about 0.07 s) where small noise matters. Throttling is possible but was not measured. The Windows build was only tested in a virtual machine, so no Windows timings are claimed. Launch-to-ready time is reported separately in the app footer (Windows VM: ' + LAUNCH_VM + ').'),

      H('4. Embedded design'),
      P('The design (docs/embedded_design.md) separates three voltage domains for the microcontroller, FPGA and JTAG adapter. The adapter drives TCK, TMS and TDI, the target drives TDO, and VTref is sensed, not supplied. A JTAG adapter is a debug interface: it runs no application and its telemetry describes the link. No shared connector, voltage or protocol is assumed. The C++ mock adapters demonstrate discovery, capabilities, connect, disconnect, telemetry and a recoverable timeout with retry; everything is labelled SIMULATED.'),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 40 }, children: [img('docs/img/block_diagram.png', 640, 357)] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [run('Fig. 2: block diagram (design only).', { italics: true })] }),

      H('5. Backpropagation'),
      P('With x=1, y=2, w=0.5, b=0, v=1, c=0 and learning rate 0.1: z=0.5, h=0.4621, y_hat=0.4621, J=1.1825. The gradients are dJ/dv=-0.7107, dJ/dc=-1.5379, dJ/dw=dJ/db=-1.2095, all negative, so every parameter increases and the prediction moves toward the target. After one simultaneous update J=0.6858. Central differences (epsilon 1e-6) agree with the analytic gradients to 1.1e-10 (limit 1e-5). Over 100 steps the loss falls monotonically to 1.7e-20 (Fig. 3); one sample can be fitted exactly, so this shows correct descent, not generalisation.'),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 20 }, children: [img('docs/img/backprop_loss.png', 440, 266)] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [run('Fig. 3: loss over 100 gradient-descent steps (log scale).', { italics: true })] }),

      H('6. Test outcomes'),
      B('Five CTest suites (FEA, parsers, batch, backprop, devices) pass on Linux (GCC 13), Windows (MSVC, in CI) and macOS (Apple Clang 17).'),
      B('Negative tests: empty, truncated, binary, non-finite and malformed STL/CSV, invalid FE inputs, cancellation, device errors. A ThreadSanitizer run found no data races.'),
      B('Packaged Launch.exe was smoke-tested in CI and launched by double-click on a Windows 11 ARM virtual machine (x64 emulation).'),
      B('Tested limits: ASCII STL up to 190,000 triangles (45 MB) and CSV up to 1,000,000 rows (40 MB) accepted; larger files are rejected with a message.'),

      H('7. Limitations'),
      B('Parts of the code were written with AI assistance (docs/ai_use_log.md); I reviewed and tested it and can explain it.'),
      B('Hardware is simulated; the voltage table is a set of assumptions, not a verified design.'),
      B('1D, constant-property, steady-state thermal model; ASCII STL only; example data are synthetic, not ANSYS exports.'),
      B('Launch.exe is unsigned, so SmartScreen may warn. Browser rendering time for the largest files and Windows benchmark timings were not measured.'),

      new Paragraph({ children: [new PageBreak()] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [new TextRun({ text: 'Appendix: handwritten backpropagation (page 1 of 2)', font: FONT, size: 28, bold: true })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, children: [img('docs/handwritten/backprop_page1.jpg', 540, 912)] }),
      new Paragraph({ children: [new PageBreak()] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [new TextRun({ text: 'Appendix: handwritten backpropagation (page 2 of 2)', font: FONT, size: 28, bold: true })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, children: [img('docs/handwritten/backprop_page2.jpg', 555, 912)] }),
    ] }],
});
Packer.toBuffer(doc).then(b => { fs.writeFileSync('docs/report.docx', b); console.log('wrote docs/report.docx'); });
