# AI-use log

**DRAFT prepared from the real working session. The candidate must read it, correct anything inaccurate, and add their own edits and decisions before submission. Nothing here is invented, and entries marked TODO have not happened yet.**

**Tool:** Claude (Anthropic), via Claude Code / claude.ai session, model reported as `claude-sonnet-5-5` (the serving model may differ). Commits made by the assistant carry a `Co-Authored-By: Claude` trailer.

## 1. Representative prompts and outputs

| # | My prompt (verbatim or close) | What the AI produced | What I did with it |
|---|---|---|---|
| 1 | Attached the brief; "plan how can we do the project, state the requirements which I would need to bring in order to complete this project successfully with your help" | A plan: C++17 backend as a local HTTP server, web UI, `Launch.exe` launcher; a 15-day schedule; a list of what only I can do (handwritten backprop, VM launch test, honest AI log) | Accepted the architecture; the schedule had to be replaced (see 2a) |
| 2 | "is it required to have an exe file rather I can show it as a website result" | Cited the brief's one-click launch requirement and shortlisting gate; a hosted website alone cannot satisfy it | Checked the wording in the brief myself; kept Launch.exe |
| 3 | "I received it on 31/10/2026, submission is in 2 days" (with the forwarded mail) | A compressed ~25-hour schedule, plus a way to build and test Windows output from a Mac (GitHub Actions + Windows 11 ARM VM) | Accepted; set up GitHub repo and UTM VM |
| 4 | "can it be because repo is private because asked it to be private" | Explained the connector could not see private repos not granted to it; suggested making the repo public during development | Made the repo public temporarily |
| 5 | "can't we produce real data ANSYS export" | Declined to fabricate an "ANSYS export"; pointed out the brief allows a self-created synthetic CSV if clearly labelled; offered to use a real export if I supplied one | Chose to keep synthetic data, labelled as such |
| 6 | "continue with synthetic and continue your task" | Built Task B parsers, Task D pool and benchmark, Task F backprop, Task E mocks, server and UI, with tests | Reviewing the code; see section 4 |
| 7 | "launch.exe is working but I don't have data" | Explained the app starts empty and the examples load from buttons; I then confirmed the `data/` folder was present and the examples worked | Confirmed on the Windows VM |

## 2. Incorrect or unsuitable suggestions I identified

a. **Schedule based on a 15-day timeline.** The first plan assumed the brief's nominal duration. My real deadline was about 41 hours away (mail: 02-11-2026, 10:00 AM). I supplied the deadline and the plan was cut down.

b. **Wrong test expectation (caught by the test suite).** The assistant wrote a parser test expecting the synthetic CSV's maximum temperature to be 100.5 °C. The data actually peaks near 188.5 °C (the rod profile's maximum), so the test failed. The assistant had assumed the maximum was at x = 0 and had not checked against the profile 100 − 80x + 500x(1 − x). The test and `data/README.md` were corrected.

c. **"UTM's built-in Windows guide" does not include the Windows ISO.** I asked to avoid external downloads; the assistant corrected itself and said a separate ISO download (CrystalFetch) is unavoidable.

d. **GitHub "Artifacts" download.** The assistant told me to click the artifact in the Actions page; as a signed-out user in the VM I had nothing to click (artifacts need a GitHub login). The workflow was changed to also publish a plain-download pre-release for testing.

e. **First Windows CI run failed** because the packaging step wrote into `docs/` before it existed (the assistant's script bug). Found from the job-step status, fixed and re-run; the second run was green.

f. **First block diagram had overlapping connector lines and text overflowing its boxes.** I looked at the rendered image (not just the SVG source), saw the problems, and the diagram was redrawn.

g. **Large-upload test returned HTTP 413 for every file** when sent with curl's default content type. With `Content-Type: text/plain` (what the browser's `fetch` with a string body sends) the same files were accepted. The cause was not investigated in the library source; I treat it as unverified. TODO: re-test a large file through the real browser UI.

## 3. Verification evidence (what I ran, not what I was told)
- `ctest`: FEA (BCs, nodal and midpoint errors, order 2.00), parsers (positive and negative), batch (serial = parallel, order, cancel), backprop (gradient check ~1e-10 vs 1e-5 tolerance), devices (errors and recovery).
- ThreadSanitizer build of the batch tests: no data races reported.
- Headless-Chrome run of every UI view, including node inspection, malformed-file banners, form validation, benchmark and device recovery.
- GitHub Actions on Windows: MSVC build, tests, static-runtime check, packaged-`Launch.exe` smoke test (green).
- Launch on a clean Windows 11 ARM VM (x64 emulation): `Launch.exe` opens the app, examples load; closing the tab made `Launch.exe` disappear from Task Manager within 25 s. Screenshots and startup timing (server ready 6 ms, UI loaded 429 ms) are in `docs/evidence/`.
- Built and tested on my MacBook Air (Apple M4, Apple Clang 17): all 5 test suites passed. Two benchmark runs are in `results/benchmark_run1.csv` and `benchmark_run2.csv` (4-worker speedup 3.51 and 3.67; serial/parallel difference 0).
- Handwritten backprop pages: I wrote them by hand. The assistant supplied a line-by-line worked outline when I asked what to write, so the structure and wording follow it; I then compared my numbers with `results/backprop_report.txt` (the C++ output) and the assistant's review of my photo pointed out minus signs that look like plus signs and page show-through. TODO: after I retake the photos, state here what I changed.

## 4. My edits and decisions
- Chose synthetic example data and its labelling.
- Chose the VM-based launch test and installed UTM and Windows 11 ARM myself.
- Chose to make the repository public temporarily and will remove the test release before submission.
- Ran the Mac build and benchmark myself, and made a real commit of the results from my machine.
- Decided to anonymise the git author (history rewritten once, tree identical) and to use my roll number RA2311056010232 as the candidate ID.
- TODO: list code I changed or re-read and can explain (FEA assembly and BCs, midpoint error, thread pool, backprop chain rule).

## 5. Deliberate challenge test
Not needed for the "incorrect suggestion" requirement (section 2 has real cases). Optional TODO: change the number of elements or a boundary condition and predict the new midpoint error before running.
