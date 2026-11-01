# Task C notes: assumptions, units, boundary conditions and a debugging insight

## Problem and units
Steady heat conduction in a uniform rod, −d/dx(kA dT/dx) = qA, 0 ≤ x ≤ L, with L = 1 m, A = 1e-4 m², k = 10 W/(m·K), q = 10,000 W/m³ (volumetric generation), T(0) = 100 °C and T(L) = 20 °C. Units are m, m², W/(m·K), W/m³ and °C throughout, so qA has units W/m and kA·dT/dx has units W.

## Assumptions
- Constant k, A and q; one-dimensional conduction; lateral surfaces insulated (no convection or radiation); steady state.
- Exact solution: T(x) = T(0) + (T(L) − T(0))x/L + q/(2k)·x(L − x).

## Finite-element formulation
- Two-node linear elements of length h = L/N. Element stiffness (kA/h)·[[1, −1], [−1, 1]]; consistent element load (qAh/2)·[1, 1].
- Global K and f are assembled by adding each element's contribution to its two nodes (the matrix is tridiagonal).
- **Boundary conditions (Dirichlet):** the known temperatures at nodes 0 and N are moved to the right-hand side of the neighbouring rows (f₁ −= K₁₀·T(0), f_{N−1} −= K_{N−1,N}·T(L)); the constrained rows are then replaced by identity rows with f = prescribed value, so the matrix stays well-posed and the boundary values are reproduced exactly. The system is solved with the Thomas algorithm.
- **Error measure:** the maximum |T_FE − T_exact| at **element midpoints**, where T_FE is the FE interpolation (average of the two nodal values), alongside the nodal error.

## Results (see `results/fea_convergence.csv`)
| N | h [m] | max nodal error [°C] | max midpoint error [°C] | observed order |
|---|---|---|---|---|
| 10 | 0.1 | 2.8e-14 | 1.25 | – |
| 20 | 0.05 | 2.0e-13 | 0.3125 | 2.00 |
| 40 | 0.025 | 8.8e-13 | 0.078125 | 2.00 |
| 80 | 0.0125 | 1.2e-12 | 0.01953125 | 2.00 |

The midpoint error equals (q/(2k))·h²/4 exactly, so it falls by a factor of 4 each time the mesh is halved (second-order convergence). The unit test checks this closed form.

## One debugging insight
**Nodal agreement hides the discretisation error.** With a quadratic exact solution, linear elements give nodal values that are exact to round-off (about 1e-14 to 1e-12), so a check at nodes alone would suggest an error-free solver and tell us nothing about mesh convergence. The error only shows up *between* nodes, where the piecewise-linear interpolation cuts the parabola's corners. Measuring at element midpoints with FE interpolation exposes it and gives the expected h² slope. The round-off in the nodal error grows slowly with N, which is consistent with accumulated floating-point error, not a modelling error.

## Limitations
Constant properties only, 1D, Dirichlet conditions only, and a dense uniform mesh. Heat flux (derivative) accuracy is not reported; it is only first-order for linear elements.
