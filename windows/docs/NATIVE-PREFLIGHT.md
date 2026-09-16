# Native dependency preflight

Executed 2026-09-06 on Windows, without elevation or hardware workloads.
This isolated build probe does not advance a roadmap gate or implement the product agent.

## Reproduce

```powershell
./tools/build-agent.ps1
./tools/build-agent.ps1 -Configuration Release
```

The script discovers MSVC and bundled CMake with Visual Studio Installer's vswhere,
configures an x64 Visual Studio build, builds it, and runs CTest. Downloads, generated
projects and executables remain under the ignored `build/` directory. First configure
requires network access. No installation, persistent database or scratch workload is
created. The executable exits after its checks; it never opens a listening socket.

## Observed environment and scope

- Visual Studio 2019 BuildTools, MSVC compiler 19.29.30159.0, toolset directory 14.29.30133.
- Bundled CMake 3.20.21032501-MSVC_2; MSBuild 16.11.6.
- Windows SDK 10.0.19041.0 selected; host version reported by CMake 10.0.26200.
- C++20 compiler mode and `std::span` compiled and executed.
- SQLite in-memory database opened, prepared `SELECT 6 * 7`, and returned 42.
- Crow `/health` handler dispatched in process and its JSON response decoded and checked.
- Crow WebSocket route instantiated and validated, with an always-reject admission callback.
  This establishes compilation, not WebSocket network interoperability.
- Project code uses `/W4 /WX`, strict conformance, UTF-8 and RAII ownership of SQLite
  handles. Only fetched dependency headers use external-header warning isolation.

Debug CTest passed 1/1 in 0.07 seconds. Release result is recorded below after execution.
The executable's success message identifies every check and the SQLite runtime version.

## Dependency pins

Every archive URL and SHA256 is recorded in `agent/CMakeLists.txt`; CMake verifies the
archive before extraction. Checksums were computed from the retrieved official upstream
archives; they provide reproducibility, not an independent publisher signature.

| Dependency | Pin | Upstream license |
| --- | --- | --- |
| [Crow](https://github.com/CrowCpp/Crow/tree/v1.2.1.2) | v1.2.1.2 | BSD-3-Clause |
| [Standalone Asio](https://github.com/chriskohlhoff/asio/tree/asio-1-30-2) | 1.30.2 | Boost Software License 1.0 |
| [nlohmann JSON](https://github.com/nlohmann/json/tree/v3.12.0) | 3.12.0 | MIT |
| [SQLite amalgamation](https://www.sqlite.org/download.html) | 3.53.4 | Public domain |

The source-only FetchContent arrangement avoids dependency examples and installation
targets. SSL/compression are not enabled. SQLite extension loading is disabled.
This is a preflight choice; the product dependency policy still needs a documented
decision on FetchContent versus the planned vcpkg baseline and a distribution notice audit.

## Limits and follow-up

The production target remains VS2022 v143, Windows 11 SDK and CMake >=3.28 in
[TOOLCHAIN.md](TOOLCHAIN.md). This run proves only the features compiled here on VS2019,
not complete C++20 compatibility or the production toolchain gate.

The old CMake does not map SYSTEM include paths to MSVC external include paths; explicit
`/external:I` is necessary. MSBuild also emits D9025 for its inherited `/external:W4`
being overridden by `/external:W0`. This diagnostic concerns external warning settings;
project warnings remain errors. No warning suppression is applied to project sources.

No live telemetry, persistent run store, canonical C++ schema decoder, transport
envelopes, command endpoints, Host/Origin policy, token bootstrap, WebSocket first-message
authentication/deadline, HTTP parser hardening, launcher, or hardware adapter is provided.
The health route has no network exposure and must not be reused as a product listener
without the security policies and tests in [16-SECURITY-PRIVILEGE-MODEL.md](16-SECURITY-PRIVILEGE-MODEL.md).
No hardware gate, remote CI result, installer or distribution approval is implied.
