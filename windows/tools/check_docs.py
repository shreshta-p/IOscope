"""Check required baseline documents and local Markdown links."""
from pathlib import Path
import re
import os

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parent
docs = ROOT / "docs"
required = [
    "00-PRODUCT-CHARTER", "01-PRD", "02-SYSTEM-ARCHITECTURE", "03-UI-UX-SPEC",
    "04-3D-DIGITAL-TWIN-SPEC", "05-TELEMETRY-SPEC", "06-WORKLOAD-ENGINE-SPEC",
    "07-EXPERIMENT-SPEC", "08-SAFETY-SPEC", "09-RUN-REPLAY-SPEC",
    "10-ANALYZER-SPEC", "11-LEARN-SPEC", "12-DATA-MODEL", "13-ERROR-HANDLING",
    "14-OBSERVABILITY", "15-ASSET-PROVENANCE", "16-SECURITY-PRIVILEGE-MODEL",
    "17-PERFORMANCE-BUDGET", "18-TEST-STRATEGY", "19-ROADMAP", "20-PORTFOLIO-DEMO",
    "CURRENT-STATE", "ASSET-PROVENANCE", "TOOLCHAIN", "BACKLOG", "ASSUMPTIONS",
    "RESEARCH", "PHASE-0-VALIDATION"
]
for name in required:
    path = docs / f"{name}.md"
    assert path.exists() and len(path.read_text(encoding="utf-8")) > 80, path
assert (ROOT / "AGENTS.md").exists()
assert len(list((docs / "adrs").glob("ADR-*.md"))) >= 5
links = 0
markdown = []
for directory, subdirs, files in os.walk(REPOSITORY):
    subdirs[:] = [name for name in subdirs if name not in {'.git', 'node_modules', 'out', 'dist', 'build', '.legacy-build', '.venv'}]
    markdown.extend(Path(directory) / name for name in files if name.endswith('.md'))
for path in markdown:
    content = path.read_text(encoding="utf-8")
    for link in re.findall(r"\[[^\]]*\]\(([^)]+)\)", content):
        if "://" in link or link.startswith("#"):
            continue
        target = link.split("#", 1)[0]
        assert (path.parent / target).resolve().exists(), f"{path}: {link}"
        links += 1
print(f"PASS: {len(required)} required docs, ADRs, AGENTS.md; {links} local links")
