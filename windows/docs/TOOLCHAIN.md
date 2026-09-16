# Tested development toolchain

Run these commands from `windows/`, not the Git repository root. After cloning,
first run `cd windows`. Install dependencies here; do not reuse a Linux install or
an old CMake build tree created before the workspace move.

Updated 2026-09-08. This Windows workspace uses Node 24.20.0, npm 11.19.0,
Python 3.13, MSVC 19.29 (VS2019 Build Tools), Windows SDK 10.0.19041 and the
bundled CMake 3.20. The build script discovers Visual Studio using vswhere and
selects the matching VS2019/VS2022 generator. This is evidence for the implemented
C++20 subset, not a claim of full VS2019 C++20 conformance.

```powershell
npm ci
python -m pip install -r tools/requirements.txt
npm run verify
./tools/build-agent.ps1 -Configuration Release
npx tsx tools/native-recording-check.ts
npm run build
./build/native-vs16/Release/ioscope_agent.exe .
```

`npm run dev` provides the development UI with same-origin API proxy. Native build
also creates `ioscope_prepare.exe`, required beside the agent. It isolates scratch
initialization and flushing from the parent's watchdog. CTest runs fake processes
and tiny owned file fixtures; no benchmark is automatically run.

Package-lock pins UI dependencies. Native CMake FetchContent pins archive hashes
for Crow 1.2.1.2, Asio 1.30.2, nlohmann/json 3.12.0, jsoncons 1.7.0 and SQLite
3.53.4. Sources populate under ignored build directories. JSONcons validates
2020-12 contracts. SQLite runs WAL, full synchronization and foreign keys.

DiskSpd 2.3 is a separate licensed dependency, pinned in agent/diskspd.lock.json.
Read the release license before running tools/setup-diskspd.ps1 with its explicit
acceptance switch. Do not include the Microsoft release binary in distributions.

Crow's parser header is modified during configuration to cap HTTP bodies at 32 MiB
before allocation. This modification is idempotent and does not change other parser
semantics. Retain Crow's BSD-3-Clause notice with any future distribution of this
modified dependency. Complete dependency notices and installer/release tests remain
Phase 11 work. Build success currently includes a UI bundle-size warning.

See [native preflight](NATIVE-PREFLIGHT.md) and
[telemetry/workload evidence](PHASE-5-6-VALIDATION.md) for observed results.
