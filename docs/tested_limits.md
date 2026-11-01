# Tested limits

Measured with `Launch` server parsing and JSON building, on the build sandbox (2 vCPU Xeon, Linux, GCC 13, -O3). Browser rendering time is not included. Windows/M4 numbers will differ.

| Input | Size | Result | Server time |
|---|---|---|---|
| ASCII STL, 100,000 triangles | 24 MB | accepted | 0.5 s |
| ASCII STL, 190,000 triangles | 45 MB | accepted | 1.2 s |
| ASCII STL, 260,000 triangles | 62 MB | rejected: "larger than the supported limit of 50 MB" | 0.2 s |
| Nodal CSV, 500,000 rows | 20 MB | accepted | 0.8 s |
| Nodal CSV, 1,000,000 rows | 40 MB | accepted | 1.5 s |
| Nodal CSV, 1,200,000 rows | 49 MB | rejected: "More than 1000000 rows" | 0.9 s |

Configured limits: 50 MB per upload, 500,000 triangles, 1,000,000 CSV rows, benchmark workload <= 4e8 element-solves, 1,000,000 FE elements. Binary STL is rejected with an explanatory message.

Through the real browser UI (headless Chrome with software WebGL on the same sandbox, file picker to first render, no GPU):

| Input | Time to first render |
|---|---|
| ASCII STL, 190,000 triangles (45 MB) | 3.6 s |
| Nodal CSV, 1,000,000 rows (40 MB) | 3.5 s |

No page errors occurred. Not measured: frame rate while rotating, browser memory use, and timings on Windows or a GPU.
