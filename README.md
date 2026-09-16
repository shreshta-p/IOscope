# IOscope

Systems performance digital twin and workload laboratory, organized as two isolated
platform workspaces in one Git repository.

| Workspace | Contents | Status |
| --- | --- | --- |
| [windows/](windows/README.md) | Existing C++ agent, React UI, contracts, simulation, tools and specifications | Phases 0-5 verified; Phase 6 real-run validation outstanding |
| [linux-vm/](linux-vm/README.md) | Linux VM development handoff and port plan | Planning only; no Linux application implemented or validated |

```text
IOscope/
  AGENTS.md                 Repository-wide engineering instructions
  CLAUDE.md                 Imports the same instructions for Claude Code
  .github/workflows/        CI with explicit platform working directories
  windows/
    agent/
    apps/ui/
    contracts/
    simulation/
    tools/
    docs/
    package.json
    package-lock.json
  linux-vm/
    AGENTS.md
    CLAUDE.md
    README.md
    docs/
```

Open the relevant platform folder in your editor. There is no root npm workspace:
dependency installation, builds, tests and generated output belong to each platform.
Do not connect them with workspace dependencies, symlinks or runtime imports.
Portable code can be deliberately ported later, with its origin documented and
wire compatibility checked against versioned contracts.

## Windows quick start

From the repository root, in PowerShell:

```powershell
cd windows
npm ci
python -m pip install -r tools/requirements.txt
npm run verify
npm run build
./tools/build-agent.ps1 -Configuration Release
./build/native-vs16/Release/ioscope_agent.exe .
```

The example executable path is for VS2019; VS2022 builds use `native-vs17`.
The agent serves the UI at http://127.0.0.1:8765. Starting the agent does not start
a workload. See the [Windows toolchain](windows/docs/TOOLCHAIN.md) for prerequisites.

## Continue in Linux

After authenticating to GitHub with access to the private repository:

```bash
git clone https://github.com/shreshta-p/IOscope.git
cd IOscope
git switch -c codex/linux-port
cd linux-vm
```

The initial Windows checkpoint is tagged `windows-baseline-2026-09-16`.

Clone this repository into the guest's local filesystem, open `linux-vm/`, and read
its [current state](linux-vm/docs/CURRENT-STATE.md) and [port plan](linux-vm/docs/PORT-PLAN.md).
The Windows agent cannot run natively on Linux. No Linux build commands are promised
until that implementation exists. VM measurements must identify guest scope.

## Handoff

Start any coding session with [AGENTS.md](AGENTS.md), then the target platform's
instructions and current state. Record decisions, commands, evidence and remaining
work in the repository so development does not depend on a particular AI agent,
editor, chat history or personal skill installation.

V1 is unfinished. No distribution license has been selected. DiskSpd binaries,
dependencies, recordings, scratch files and build outputs are excluded from Git.
