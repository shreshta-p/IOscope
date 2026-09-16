# IOscope for Linux VM

Independent workspace for developing the Linux version inside VirtualBox.
**Status: planning only. No Linux source implementation, package manifest, native
executable or validated Linux build exists yet.**

## Start a development session

1. Clone the repository into the Linux guest's own filesystem, such as `~/src/IOscope`.
2. Open `linux-vm/` in your editor and use any coding agent you prefer.
3. Read [AGENTS.md](AGENTS.md), [current state](docs/CURRENT-STATE.md) and
   [the port plan](docs/PORT-PLAN.md).
4. Implement and verify the first port milestone, updating current state with evidence.

There are no Linux install or run commands yet. Do not execute the sibling Windows
PowerShell scripts as Linux setup or share its `node_modules` or CMake build caches.
The [Windows implementation](../windows/README.md) is the reference baseline.

Keep this port independently buildable. When bringing over portable UI, simulation
or contract code, copy it deliberately, document its baseline, retain notices and
add compatibility checks. Do not add imports or build paths into `../windows/`.

## VM scope

Functional checks in a VM can validate guest behavior, recovery and cancellation.
They cannot certify physical-host disk performance or unavailable host sensors.
Record that the environment is virtualized, retain `live` origin for actual guest
measurements and present virtualization context separately. Bare-metal Linux
validation will be a separate gate.

Use small, explicit file-based trials. The Windows default is 64 MiB and five seconds
of reads after preparation; this is a starting constraint, not a passed Linux test.
