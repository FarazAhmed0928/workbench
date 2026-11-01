# Clean-machine launch evidence

Taken on a Windows 11 Pro 24H2 (build 26100) ARM64 virtual machine (UTM on an Apple M4 Mac) with no compiler, editor or developer tools installed. The x64 `Launch.exe` ran under Windows' built-in x64 emulation. The VM has 4 logical CPUs.

| File | What it shows |
|---|---|
| `01_winver_windows11.png` | Windows version (the licensee name was masked) |
| `02_thermal_analysis_windows_vm.png` | Thermal Analysis after Solve: h = 0.1 m, max nodal error 2.84e-14 C, max midpoint error 1.25 C, plot with units; page served from 127.0.0.1 |
| `03_nodal_csv_windows_vm.png` | Synthetic nodal-temperature CSV loaded: 273 nodes, 20 to 188.5 C, legend with units |

Observed on 01-11-2026 (timestamps are visible in the taskbar):
- Double-clicking `Launch.exe` opened the default browser with the workbench.
- Startup line in the app footer: server ready 6 ms, UI loaded 429 ms after process start.
- Closing the browser tab made `Launch.exe` disappear from Task Manager within the 25 s wait.
