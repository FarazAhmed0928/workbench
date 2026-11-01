#!/usr/bin/env python3
"""Pre-submission checks on an extracted package folder.
Usage: python3 scripts/check_submission.py <folder>
Flags missing required items, machine-specific absolute paths, personal data and credentials."""
import os, re, sys

root = sys.argv[1] if len(sys.argv) > 1 else "."
required = ["Launch.exe", "README.md", "CMakeLists.txt", "web/index.html", "src/core/fea1d.cpp", "src/server/main.cpp",
            "tests/test_fea.cpp", "data/README.md", "results/fea_convergence.csv", "docs/block_diagram.png",
            "docs/embedded_design.md", "docs/ai_use_log.md", "docs/ownership_statement.md", "docs/commit_log.txt",
            "workbench.bundle", "third_party/THIRD_PARTY.md", ".vscode/tasks.json", ".vscode/launch.json"]
problems = []
for r in required:
    if not os.path.exists(os.path.join(root, r)):
        problems.append(f"MISSING required item: {r}")

patterns = {
    "absolute path": re.compile(r"(/home/\w+|/Users/\w+|[A-Za-z]:\\Users\\|/mnt/user-data|/tmp/claude)"),
    "email address": re.compile(r"[\w.+-]+@[\w-]+\.[\w.]+"),
    "phone number": re.compile(r"\+?\d[\d\s-]{9,}\d"),
    "credential": re.compile(r"(ghp_[A-Za-z0-9]{20,}|github_pat_\w+|AKIA[0-9A-Z]{16}|BEGIN (RSA|OPENSSH) PRIVATE KEY)"),
}
skip_dirs = {".git", "vendor", "third_party"}
text_ext = {".md", ".txt", ".cpp", ".hpp", ".h", ".js", ".html", ".css", ".json", ".yml", ".csv", ".svg", ".py", ".sh"}
for d, dirs, files in os.walk(root):
    dirs[:] = [x for x in dirs if x not in skip_dirs]
    for f in files:
        p = os.path.join(d, f)
        if os.path.splitext(f)[1].lower() not in text_ext or os.path.getsize(p) > 5_000_000:
            continue
        try:
            txt = open(p, encoding="utf-8", errors="ignore").read()
        except OSError:
            continue
        for name, rx in patterns.items():
            for m in rx.finditer(txt):
                if name == "phone number" and f.endswith(".csv"):
                    continue
                line = txt.count("\n", 0, m.start()) + 1
                problems.append(f"{name}: {os.path.relpath(p, root)}:{line}: {m.group(0)[:60]}")
                break
print("\n".join(problems) if problems else "No problems found.")
sys.exit(1 if problems else 0)
