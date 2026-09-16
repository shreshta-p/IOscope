# Platform separation and handoff

Updated 2026-09-16. User requested isolated `windows/` and `linux-vm/` folders before
committing or pushing the repository.

## Layout decisions

The complete existing application moved under `windows/`: agent, UI, schemas,
simulation, tools, specifications and npm workspace configuration. Internal relative
paths remain stable. Run all Windows commands from this directory. Historical
documents referring to the project/repository root mean this Windows workspace for
source, build and output paths; old absolute paths are historical evidence only.

`linux-vm/` contains independent instructions, current state and a port plan. No
Linux implementation or validation is claimed. Future ports must remain locally
buildable, with explicit provenance and compatible versioned contracts. No root
shared-code workspace or platform-crossing imports were introduced.

Repository metadata, global rules and CI remain at the Git root. CI now selects
`windows/` for commands and uses `windows/package-lock.json` for its npm cache.
The documentation checker covers root and both platform folders.

## Local generated files

Installed dependencies and prior output moved into `windows/`. Workspace links must
be regenerated with `npm ci`. Old native build caches embed the former absolute
source paths, so they were preserved under ignored `windows/.legacy-build/` for
local reference, not reused as the new build tree. New builds use `windows/build/`.
The existing pinned DiskSpd directory moved to `windows/build/`; it remains ignored
and is not redistributed or executed as part of reorganization.

The move was checked with SHA-256 hashes for 192 source/document files before the
necessary documentation and CI edits. The local manifest is in ignored
`windows/out/restructure-source-manifest.json`.

## Validation

Fresh checks on 2026-09-16, run from `windows/`:

- `npm ci`: successful; workspace junctions resolve inside `windows/`.
- `npm run verify`: passed generated-contract drift, formatting, TypeScript,
  118 tests in nine files, 25 schemas, 24 root fixtures, one unavailable fixture,
  27 rejected invalid mutations, the 12-snapshot golden recording and document links.
- `npm run build`: passed; the existing large UI bundle warning remains.
- `./tools/build-agent.ps1 -Configuration Release`: fresh VS2019/MSVC build and
  all 10 native CTest programs passed, without reusing the old CMake cache.
- `npx tsx tools/native-recording-check.ts`: all four fake-adapter native outcomes
  pass TypeScript recording validation.
- `node tools/native-hosting-e2e.mjs`: actual port 8765 returns the homepage and
  built assets, rejects invalid asset paths and mounts Workload Lab in Chrome.
  No workload was started.
- Git ignore checks exclude native builds, the preserved old cache, DiskSpd,
  installed dependencies and local evidence. All 192 original files remain present.

The first `npm run verify` stopped at formatting because Prettier's root `*.json`
glob reported no matches in the relocated workspace. Explicit filenames worked;
the scripts now enumerate package.json, package-lock.json, tsconfig.json and
.prettierrc.json. The full verification passed after this change.

Local logs: `out/restructure-verify.log`, `out/restructure-native-build.log`,
`out/restructure-ui-build.log`; native hosting screenshot: `out/native-hosting.png`.
These are ignored local artifacts. Earlier performance measurements remain historical;
no performance soak, real workload trial or Linux test was rerun for the move.

## Remaining boundary

No hardware workload is part of this move. Windows Phase 6 remains untested on real
hardware. Linux progress is tracked separately. This reorganization does not commit,
push, publish, select a distribution license or pass a release gate.
