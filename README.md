# Unified Engineering Workbench

A local engineering workbench with a **C++17 backend** and a **browser front end**. It covers CAD/ANSYS-style visualisation, a 1D thermal finite-element solver, a multithreaded benchmark, simulated embedded-device adapters and a hand-derived backpropagation example.

## Prerequisites (to run the packaged app)
- Windows 11 x64. **No installation, no compiler, no internet and no admin rights are needed.**
- A default web browser (Edge, Chrome or Firefox, with WebGL).

## One-click run
1. Extract the whole ZIP to a normal folder (do not run it from inside the ZIP).
2. Double-click **`Launch.exe`**. No terminal is needed. Your browser opens the workbench automatically.
3. Close the browser tab (or click **Quit**) and the server stops by itself after about 20 seconds.

If Windows says "Windows protected your PC" (the exe is not code-signed), choose **More info → Run anyway**, or right-click the ZIP → Properties → **Unblock** before extracting.

The server listens on `127.0.0.1` only (not reachable from the network) and picks a free port, so no firewall prompt appears.

## What is in the UI
| View | What it does |
|---|---|
| Geometry → STL | Loads an **ASCII STL**; rotate, pan, zoom, wireframe, fit to view |
| Geometry → Nodal CSV | Loads nodal temperatures; colour legend, click-to-inspect node, PNG export |
| Thermal Analysis | 1D rod FE solve, N = 10/20/40/80 convergence study, plots, CSV and PNG export |
| Performance | Serial vs parallel batch benchmark with progress, cancel and CSV export |
| Hardware | **Simulated** MCU, FPGA and JTAG adapters, with a recoverable timeout |
| Backprop (Task F) | Hand-derived gradients, finite-difference check and loss curve |

## Supported formats
- **STL:** ASCII only (binary STL is rejected with an explanation). Triangles only. No unit field: coordinates are shown as model units.
- **Nodal CSV:** header exactly `node_id,x_m,y_m,z_m,temperature_C`; metres and °C; `#` comment lines allowed; duplicate ids, non-finite values and wrong column counts are rejected with the line number.
- Limits: 50 MB per file, 500,000 triangles, 1,000,000 rows (see `docs/tested_limits.md`).
- Bundled examples in `data/` are **synthetic** (see `data/README.md`). They are not ANSYS exports.

## Architecture
```
web/            browser UI (plain JS, Three.js and Chart.js bundled locally in web/vendor)
src/core/       wb_core static library: FEA, STL/CSV parsers, thread pool, batch, benchmark,
                backprop, mock devices, SVG plots. No UI or HTTP code.
src/server/     Launch.exe: HTTP/JSON transport (cpp-httplib) + launcher. No numerical logic.
tests/          unit tests (positive, negative, numerical) run by CTest
tools/          example-data generator, results generator, CLI benchmark
data/           example inputs with provenance
results/        raw CSVs and labelled plots produced by the code
docs/           design notes, block diagram, report, AI-use log, ownership, tested limits
third_party/    vendored header-only libraries with versions and hashes
```

## Build and test from source
Requires CMake ≥ 3.16 and a C++17 compiler (MSVC 2022, GCC, or Apple Clang).
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/bench            # benchmark CLI: writes results/benchmark.csv and benchmark_summary.txt
./build/gen_results      # regenerates FEA/backprop CSVs and plots in results/
```
On Windows, MSVC builds use the static runtime (`/MT`), so `Launch.exe` needs no Visual C++ redistributable. A GitHub Actions workflow (`.github/workflows/build-windows.yml`) builds, tests and smoke-tests the packaged Windows build.

## Editor configuration
`.vscode/` contains `tasks.json` and `launch.json` (VS Code with the C/C++ and CMake Tools extensions): "Run Workbench", "Build from source" and "Run unit tests".

## Tests
`tests/test_fea.cpp`, `test_parsers.cpp`, `test_batch.cpp`, `test_backprop.cpp`, `test_devices.cpp`: boundary conditions, nodal and midpoint errors, order of convergence, malformed/empty/binary/non-finite inputs, serial/parallel agreement (≤ 1e-8 °C), cancellation, gradient check (ε = 1e-6, ≤ 1e-5), and device error/recovery paths.

## Known limitations
- Hardware is **simulated**; there is no real USB, serial or JTAG I/O.
- STL is ASCII only; no unit conversion; no mesh repair.
- The thermal model is 1D, constant-property, steady-state.
- `Launch.exe` is unsigned, so SmartScreen may warn.
- Benchmark speedup depends heavily on the machine; results from the author's machine are in `docs/` and `results/` and are not a promise for yours.
- Browser rendering time for the largest accepted files has not been measured.

## Version control
`docs/commit_log.txt` is the readable history and `workbench.bundle` is a Git bundle (`git clone workbench.bundle workbench`). Commits written with AI assistance are recorded in `docs/ai_use_log.md`.
