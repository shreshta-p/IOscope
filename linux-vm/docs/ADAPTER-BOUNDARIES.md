# Linux adapter boundaries (L0 design, not yet implemented)

This documents the planned Linux replacements for the Windows adapters described in
[windows/docs/02-SYSTEM-ARCHITECTURE.md](../../windows/docs/02-SYSTEM-ARCHITECTURE.md),
[05-TELEMETRY-SPEC.md](../../windows/docs/05-TELEMETRY-SPEC.md),
[06-WORKLOAD-ENGINE-SPEC.md](../../windows/docs/06-WORKLOAD-ENGINE-SPEC.md) and
[08-SAFETY-SPEC.md](../../windows/docs/08-SAFETY-SPEC.md). None of this is implemented
yet; the contracts/capability-state model (available/unavailable/unsupported/
temporarily_errored) carries over unchanged from `contracts/v1/domain.schema.json`.

## Telemetry

| Signal | Windows source | Linux replacement | Notes |
|---|---|---|---|
| CPU utilization | PDH | `/proc/stat` delta between two samples | Same rate-needs-two-samples rule as the Windows spec |
| RAM | `GlobalMemoryStatusEx` | `/proc/meminfo` (`MemTotal`, `MemAvailable`) | |
| Per-disk I/O | PDH per-physical-disk counters | `/sys/block/<dev>/stat` or `/proc/diskstats` deltas | In this VM the block device is a virtio/VBox virtual disk, not physical hardware; provenance must say guest/virtual, never "physical SSD" |
| GPU | NVML | Same NVML library, loaded dynamically, only if an NVIDIA driver is present | This VirtualBox VM has no GPU passthrough — expect `unavailable`, not zero |
| Temperature | vendor sensors via NVML/WMI | `/sys/class/hwmon`, optionally `lm-sensors` | VirtualBox does not expose real thermal sensors to the guest; must stay unavailable, never invented or clamped |

Acquisition cadence, staleness window, deadline-per-adapter, and the
measured/derived/simulated/conceptual provenance rules are unchanged from the Windows
spec — they are policy, not OS-specific.

## Workload execution

Windows pins DiskSpd; Linux will pin a specific fio version (evaluated in L2, not
installed yet). Both are external processes driven by an argv the UI never controls.
fio's `--output-format=json` is the analog of DiskSpd's XML output and needs the same
bounded, fail-closed parser (malformed output → failed run with preserved artifact,
never zero-valued results). Exact option mapping and whether any DiskSpd preset
translates cleanly to fio is L2 work; matching option names is not evidence of matching
numeric behavior.

## Process lifetime and cancellation

Windows uses a suspended child assigned to a kill-on-close Job Object before resume, so
the whole descendant tree dies if the agent dies. The Linux equivalent is a cgroup v2
scope: launch the workload process into its own delegated cgroup (via a user
`systemd-run --user --scope` or an agent-managed cgroup under
`/sys/fs/cgroup/user.slice/user-<uid>.slice/user@<uid>.service/`), then use
`cgroup.kill` to terminate every process in it atomically. This avoids the PID-reuse
and re-parenting races that plain process-group signals (`kill(-pgid, ...)`) have.
Cancellation stays bounded: graceful `SIGTERM`, then `cgroup.kill` (or `SIGKILL` to the
group as a fallback if cgroup delegation isn't available) within the same 2-second
ceiling as the Windows spec. This VM's kernel (7.0.0) supports cgroup v2 `cgroup.kill`.

## Owned scratch files

Windows scratch lives under per-user LocalAppData, never OneDrive. The Linux
replacement is `$XDG_DATA_HOME/ioscope` (falling back to `~/.local/share/ioscope`),
resolved and validated the same way: reject symlink ancestors, reject UNC-equivalent
paths, require same-uid ownership on the scratch parent, and verify final resolved
path/identity before opening.

One Linux-specific check that Windows doesn't need in quite the same way: **verify the
scratch directory's filesystem is not `tmpfs`, `overlay`, `vboxsf`, or a network
filesystem** before running a disk trial. `/tmp` on this VM is `tmpfs` — writes there
never touch a disk at all, which would silently turn a disk-I/O benchmark into a
memory-copy benchmark. Admission must `statfs()`/read `/proc/mounts` for the scratch
path and refuse to proceed on a non-block-backed filesystem, per the explicit repo
instruction to avoid VirtualBox shared folders and non-owned directories for benchmark
scratch.

## Resource admission

Same policy constants as [08-SAFETY-SPEC.md](../../windows/docs/08-SAFETY-SPEC.md)
(2GiB disk reserve, 2GiB RAM reserve, bounded durations/writes) — these are
application policy, not Windows-specific. The Linux-specific caveat: guest free space
from `statvfs()` on a dynamically-allocated VirtualBox virtual disk does not prove the
*host* has that space free. This gap can't be closed from inside the guest; admission
will disclose it as a known limitation rather than pretend otherwise. Thermal admission
will almost certainly find no usable sensors in this VM and must fall back to the
"missing thermal coverage" restricted policy (light intensity, ≤15s phases) already
specified for Windows, not invent a temperature.

## Persistence

Same approach as Windows: SQLite, single writer, WAL mode, stored under the
agent-owned scratch/data directory — never under a `vboxsf` mount, both for the
same-owner/no-shared-folder rule and because network/shared filesystems handle SQLite
file locking unreliably.
