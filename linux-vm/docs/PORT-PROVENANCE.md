# Ported code provenance

## Baseline

Source: `windows/contracts/`, `windows/simulation/`, `windows/tools/generate-contracts.mjs`
and `windows/tools/export-simulation.ts` at commit `280ced5`, tag
`windows-baseline-2026-09-16`.

## Deliberate, recorded schema changes (not silent)

Two changes to `contracts/v1/domain.schema.json`, both found only once a real
end-to-end run was attempted through the actual native agent (not by any
fake-adapter test, since fixtures never carry live engine/platform values):

1. `WorkloadAdmission.engineVersion` was `{"const": "2.3"}` — DiskSpd's literal
   pinned version, hardcoded into the schema itself. Reusing "2.3" for fio's
   admission responses would misreport which engine actually ran. Relaxed to
   `{"type": "string", "minLength": 1, "maxLength": 64}`.
2. `HardwareInventory.platform`'s enum was `["windows", "simulated"]` — there was no
   valid value a Linux agent could ever report for its own inventory. Added
   `"linux"` to the enum.
3. `RunMetadata.artifacts[].kind` and the standalone `ArtifactPayload.kind` enums were
   `["diskspd-xml", "diagnostic", "recording"]` / `["diskspd-xml", "diagnostic"]` —
   fio's JSON result artifact has no valid `kind` to report as (labeling it
   `"diskspd-xml"`, as `workload_controller.hpp` did before this fix, would misreport
   the artifact's actual format). Added `"fio-json"` to both enums; changed the one
   `workload_controller.hpp` call site that hardcoded `"diskspd-xml"` to `"fio-json"`.

Both are additive/backward-compatible: every document valid under the Windows
baseline is still valid under this schema, so `schemaVersion` stays `"1.0.0"` and no
fixture needed updating — confirmed by re-running the full `npm run verify` (109
tests, typecheck, format, contract-drift check) after both edits, all passing. Per
[AGENTS.md](../AGENTS.md), "do not silently diverge under the same schema version" —
recording them here is exactly that: recorded, backward-compatible relaxations, not
silent meaning changes. `RunMetadata.engine.version` (a plain string, not a const) is
unaffected and already carries `fio_pinned_version` honestly.

## What was copied

- `contracts/v1/*.schema.json`, `contracts/flow-semantics.v1.json`,
  `contracts/metric-catalog.v1.json`, `contracts/fixtures/*.json` — unmodified.
  Schema version remains `1.0.0`; no field, unit or provenance meaning changed.
- `contracts/src/*.ts` (`index.ts` is generated, not hand-edited), `simulation/src/*.ts`,
  and their `tests/*.ts` — unmodified. These files use relative imports
  (`../../contracts/src/...`), not npm package resolution, so they work unchanged as
  long as `contracts/` and `simulation/` stay sibling directories, which this layout
  preserves.
- `tools/generate-contracts.mjs`, `tools/export-simulation.ts` — unmodified. Both are
  plain Node.js with no Windows/MSVC dependency.

## Native agent (`agent/`, baseline commit 280ced5)

`windows/agent/` is a mix of pure C++ business logic and Win32-specific adapters.
The pure logic is copied unmodified into `linux-vm/agent/`: `safety.hpp`,
`counter_math.hpp`, `store.hpp`, `json_input.hpp`, `native_recording.hpp`,
`security.hpp`, `contract_tests.cpp`, `preflight.cpp`, `counter_math_tests.cpp`,
`json_input_tests.cpp`, `safety_tests.cpp` — none of these touch a Windows API.

`contracts.hpp` needed one change, found only once a real end-to-end run was
attempted (fake-adapter tests never exercise this line, since fixtures never carry
`inventory.platform`): its native/live-origin check for `RunRecording` hardcoded
`metadata["inventory"]["platform"]=="windows"`. Changed to
`(...=="windows"||...=="linux")` — a native recording must come from a real agent
platform, never `"simulated"`; which platform that is is a port detail, not a
wire-format one.

`recording_validation.hpp` is copied with one line changed: `previousState==
metadata["outcome"]` (a `std::string` compared directly to a `nlohmann::json` value)
compiles under MSVC's STL but not GCC/libstdc++, because nlohmann-json 3.12's C++20
`operator==(ScalarType)` is constrained to `is_scalar_v`, which excludes `std::string`.
Changed to `previousState==metadata["outcome"].get<std::string>()` — same comparison,
just spelled so it resolves on both compilers.

`workload_controller.hpp` is copied with two intentional changes: the `engineVersion`
field is populated from `fio_pinned_version` (this file's Linux equivalent) instead of
the Windows-only `diskspd_version` constant, and the completed-run artifact's `kind`
is `"fio-json"` instead of the hardcoded `"diskspd-xml"`. Both fields' purpose is
unchanged (report the actual engine version; label the actual artifact format) —
only which engine/format that is.

Rewritten for Linux, with the Windows file as the design reference (same
responsibilities, same call shape, POSIX/procfs/sysfs/cgroups instead of
Win32/PDH/NVML-LoadLibrary/Job-Objects/ACLs): `platform.hpp`, `telemetry.hpp`,
`system_resources.hpp`, `scratch.hpp`, `process_job.hpp`, `main.cpp`, `prepare.cpp`,
and their test files. `fio.hpp`/`fio_json.hpp` replace `diskspd.hpp`/`diskspd_xml.hpp`
(fio replaces DiskSpd as the pinned Linux workload dependency — see
[ADAPTER-BOUNDARIES.md](ADAPTER-BOUNDARIES.md)). New, with no Windows equivalent:
`posix_handle.hpp` (RAII fd wrapper), `sha256.hpp` (self-contained SHA-256, replacing
Windows' BCrypt-backed hash so no OpenSSL dependency is needed), `linux_proc.hpp`
(shared `/proc` readers).

## What was adapted (Linux-local, not a wire change)

- Root `package.json`, `tsconfig.json`, `.prettierrc.json`, `.prettierignore` are new
  files scoped to this workspace: only the `contracts`/`simulation` workspaces and the
  dependencies they need (no `apps/ui`, no React/Vite/Playwright types, no jsx/DOM lib).
  Same TypeScript/Prettier/Vitest tool versions as the Windows baseline so behavior
  should match; `@ioscope/contracts`/`@ioscope/simulation` package names kept identical
  since the wire contract is the same, not a fork.
- `npm run verify` here omits the Python fixture validator
  (`windows/tools/validate_contracts.py`) and the Windows doc-link checker
  (`windows/tools/check_docs.py`). Neither is ported yet — see
  [CURRENT-STATE.md](CURRENT-STATE.md) for why and what covers their ground for now.

## Not copied

`apps/ui/`, `agent/`, native build tooling, PowerShell scripts, DiskSpd packaging —
these are Windows-specific (MSVC, Win32/PDH, DiskSpd) and are reference material only,
per [AGENTS.md](../AGENTS.md) and the [port plan](PORT-PLAN.md).

## Compatibility checks run

See [CURRENT-STATE.md](CURRENT-STATE.md) for exact commands and results. In summary:
regenerating `contracts/src/index.ts` from the unmodified schema reproduces the
Windows-generated file byte-for-byte, and the full ported Vitest suite (contracts +
simulation, including the golden seed-42 recording fixture) passes unmodified.
