# Timestamped screenshot walkthrough (equivalent to a demonstration video)

**How this was produced:** `scripts/walkthrough.js` drove the real application in headless Chrome (software WebGL, no GPU) on the Linux development machine, clicking through every view. It wrote the screenshots below and `walkthrough_log.txt` (UTC timestamps, the HTTP calls the page made, values read from the page). This is a scripted run on the development machine, not the Windows machine. Real Windows 11 evidence from a clean virtual machine is in `docs/evidence/`.

Re-create it with `Launch --no-browser --port 8772`, then `node scripts/walkthrough.js http://127.0.0.1:8772/ docs/walkthrough`.

| # | Screenshot | What it demonstrates |
|---|---|---|
| 01 | `01_stl_example.png` | ASCII STL loaded; triangle count and size shown |
| 02 | `02_csv_legend_and_node_inspection.png` | Nodal CSV point cloud, colour legend with units, clicked node's id, coordinates and temperature |
| 03 | `03_malformed_csv_rejected.png` | A CSV containing `nan` is rejected with a line-numbered message |
| 04 | `04_thermal_convergence.png` | N = 10/20/40/80 solutions against the exact curve; convergence table (order 2.000) and log-log plot |
| 05 | `05_thermal_validation_error.png` | Invalid conductivity (k = -3) blocked with a message |
| 06 | `06_performance_running.png` | Benchmark in progress with progress bar and Cancel enabled |
| 07 | `07_performance_result.png` | Median timings, speedup, efficiency, agreement check, system details |
| 08 | `08_hardware_simulated_recovery.png` | SIMULATED banner; MCU, FPGA and JTAG cards; injected timeout then recovery by retry; event log |
| 09 | `09_backprop.png` | Gradients, finite-difference check (max error about 1e-10) and loss curve |

Notes: timings in this run come from a 2-vCPU sandbox and are not the benchmark results (those are `results/benchmark_run1.csv` and `benchmark_run2.csv` from the Apple M4). The footer's "UI loaded" time in these screenshots reflects the slow software-rendered browser, not a real machine.
