# IOscope

Systems performance digital twin and workload laboratory, organized as two isolated
platform workspaces in one Git repository.

| Workspace | Contents | Status |
| --- | --- | --- |
| [windows/](windows/README.md) | Existing C++ agent, React UI, contracts, simulation, tools and specifications | Phases 0-5 verified; Phase 6 real-run validation outstanding |
| [linux-vm/](linux-vm/README.md) | Independent Linux C++ agent, React UI, contracts, simulation, tools and specifications | V1 complete for this platform: telemetry, workload engine, all 5 experiment types, deterministic analyzer and Learn all verified against real hardware in a VM and a real browser (Phase 10 "Ask" deferred; bare-metal validation out of scope) |

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
    agent/
    apps/ui/
    contracts/
    simulation/
    tools/
    docs/
    AGENTS.md
    CLAUDE.md
    README.md
    package.json
    package-lock.json
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

## Linux quick start

From the repository root, on the Linux guest:

```bash
cd linux-vm
npm ci
npm run verify
npm run build
cd agent
cmake -S . -B build
cmake --build build -j2   # cap parallelism; see linux-vm/AGENTS.md
ctest --test-dir build --output-on-failure
sudo apt-get install fio  # only needed to run real workloads, not to build
./build/ioscope_agent ..
```

The agent serves the UI at http://127.0.0.1:8765. Starting the agent does not
start a workload — every real fio-backed run requires explicit admission and,
in this project's own development history, explicit human opt-in. Measured
fresh-clone-to-verified time in the reference VM: ~79 seconds (see
[current state](linux-vm/docs/CURRENT-STATE.md) for the full breakdown).
The Windows agent cannot run natively on Linux; this is an independent native
implementation, not a compatibility layer. See
[current state](linux-vm/docs/CURRENT-STATE.md) and
[port plan](linux-vm/docs/PORT-PLAN.md) for exactly what's proven and how. VM
measurements describe the guest, never the physical host.

## Handoff

Start any coding session with [AGENTS.md](AGENTS.md), then the target platform's
instructions and current state. Record decisions, commands, evidence and remaining
work in the repository so development does not depend on a particular AI agent,
editor, chat history or personal skill installation.

Windows V1 is unfinished (Phase 6 real-run validation outstanding). Linux V1 is
complete for this platform, verified in a VM against real hardware (bare-metal
Linux validation is a separate, out-of-scope gate). No distribution license has
been selected. DiskSpd/fio binaries, dependencies, recordings, scratch files
and build outputs are excluded from Git.
