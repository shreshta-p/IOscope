# Ported code provenance

## Baseline

Source: `windows/contracts/`, `windows/simulation/`, `windows/tools/generate-contracts.mjs`
and `windows/tools/export-simulation.ts` at commit `280ced5`, tag
`windows-baseline-2026-09-16`.

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
