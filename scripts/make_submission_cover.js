// Builds the cover page and the launch-evidence page of the 1 MB submission PDF.
// Run from the repo root: node scripts/make_submission_cover.js
const fs = require('fs');
const { Document, Packer, Paragraph, TextRun, ExternalHyperlink, ImageRun, AlignmentType, HeadingLevel, PageBreak,
  Table, TableRow, TableCell, WidthType, BorderStyle, ShadingType } = require('docx');
const F = 'Arial';
const ZIP = 'https://github.com/FarazAhmed0928/workbench/releases/download/latest-build/CandidateID_Workbench.zip';
const REPO = 'https://github.com/FarazAhmed0928/workbench';
const t = (x, o = {}) => new TextRun({ text: x, font: F, size: 24, ...o });
const P = (kids, o = {}) => new Paragraph({ spacing: { after: 120 }, children: Array.isArray(kids) ? kids : [t(kids)], ...o });
const link = (u, label) => new ExternalHyperlink({ link: u, children: [new TextRun({ text: label || u, font: F, size: 24, color: '1F5FBF', underline: {} })] });
const H = x => new Paragraph({ heading: HeadingLevel.HEADING_2, spacing: { before: 200, after: 80 }, children: [new TextRun({ text: x, font: F, size: 28, bold: true })] });
const b = { style: BorderStyle.SINGLE, size: 4, color: '999999' }, borders = { top: b, bottom: b, left: b, right: b };
const W = [3300, 6300];
const row = (a, c, head) => new TableRow({ children: [a, c].map((x, i) => new TableCell({ width: { size: W[i], type: WidthType.DXA }, borders,
  margins: { top: 50, bottom: 50, left: 90, right: 90 }, shading: head ? { fill: 'E8EEF7', type: ShadingType.CLEAR, color: 'auto' } : undefined,
  children: [new Paragraph({ children: [t(x, { bold: !!head, size: 22 })] })] })) });
const img = (f, w, h) => new ImageRun({ type: 'jpg', data: fs.readFileSync(f), transformation: { width: w, height: h }, altText: { title: f, description: f, name: f } });

const cover = new Document({ sections: [{ properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1100, bottom: 1000, left: 1100, right: 1100 } } }, children: [
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 }, children: [t('Unified Engineering Workbench', { bold: true, size: 40 })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 240 }, children: [t('Complete assignment submission (PDF edition)', { size: 28 })] }),
  P([t('Candidate ID (roll number): ', { bold: true }), t('RA2311056010232')]),
  P([t('Date: ', { bold: true }), t('01-11-2026')]),
  H('Where the full project package is'),
  P('The submission form accepts only a PDF of at most 1 MB, so the complete package required by section 8 of the brief is hosted as a ZIP (about 13 MB, named CandidateID_Workbench.zip). It contains Launch.exe and the packaged application, src/ (frontend and C++ backend), CMakeLists.txt with dependency lock, editor run configuration, data/ with provenance, tests/, results/ (raw CSV and labelled plots), docs/, README, the Git bundle and readable commit log, and the AI-use log.'),
  P([t('Download the ZIP: ', { bold: true }), link(ZIP)]),
  P([t('Source repository: ', { bold: true }), link(REPO)]),
  P('Unzip with "Extract All" and double-click Launch.exe. No installer, compiler, terminal or internet connection is needed. Windows may show "Windows protected your PC" because the executable is unsigned: choose More info, then Run anyway.'),
  P('If either link is unavailable, please contact me through SRM Career Centre and I will supply the ZIP by another route.'),
  H('What this PDF contains'),
  P('1. Technical report (design, FEA verification, performance, embedded design, backpropagation, tests, limitations).  2. Handwritten backpropagation appendix (2 pages).  3. Task E design reference (interface table and plan).  4. Clean-machine launch evidence from a Windows 11 virtual machine.  5. AI-use log.  6. Ownership statement.'),
  H('Headline results'),
  new Table({ width: { size: 9600, type: WidthType.DXA }, columnWidths: W, rows: [
    row('Item', 'Result', true),
    row('Thermal FEA, N = 10/20/40/80', 'Max midpoint error 1.25, 0.3125, 0.0781, 0.0195 C; observed order 2.00'),
    row('Backpropagation check', 'Max gradient error about 1e-10 (limit 1e-5)'),
    row('Benchmark (Apple M4, 2 runs)', '4 workers: 3.51x and 3.67x; serial/parallel difference 0'),
    row('Tests', '5 CTest suites pass on Linux, Windows (CI) and macOS'),
    row('Clean-machine launch', 'Windows 11 ARM VM: server ready 6 ms, UI loaded 429 ms; exits when the tab closes'),
  ] }),
] }] });

const ev = new Document({ sections: [{ properties: { page: { size: { width: 11906, height: 16838 }, margin: { top: 1000, bottom: 900, left: 1100, right: 1100 } } }, children: [
  H('Clean-machine launch evidence'),
  P('Windows 11 Pro 24H2 (build 26100) ARM64 virtual machine on an Apple M4 Mac, with no compiler or developer tools installed. The x64 Launch.exe ran under Windows x64 emulation. Double-clicking Launch.exe opened the default browser with the workbench; closing the tab ended the Launch.exe process within 25 seconds (checked in Task Manager). Startup: server ready in 6 ms, UI loaded in 429 ms after process start.'),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 40 }, children: [img('docs/img_small/ev_02_thermal_analysis_windows_vm.jpg', 430, 369)] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 160 }, children: [t('Thermal Analysis in the Windows VM (units shown, midpoint error 1.25 C for N = 10).', { italics: true, size: 20 })] }),
  new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 40 }, children: [img('docs/img_small/ev_03_nodal_csv_windows_vm.jpg', 400, 395)] }),
  new Paragraph({ alignment: AlignmentType.CENTER, children: [t('Synthetic nodal-temperature CSV in the Windows VM, with colour legend in degrees C.', { italics: true, size: 20 })] }),
] }] });
Promise.all([Packer.toBuffer(cover), Packer.toBuffer(ev)]).then(([a, c]) => { fs.writeFileSync('docs/submission/cover.docx', a); fs.writeFileSync('docs/submission/evidence.docx', c); console.log('ok'); });
