# OpenMediaVault ZFS Plugin — Complete User Guide

This guide covers installation, configuration, and daily use of the openmediavault-zfs plugin.
It assumes OpenMediaVault 7 (OMV7) is already installed and operational.

---

## Table of Contents

1. [What is ZFS?](#1-what-is-zfs)
2. [Prerequisites](#2-prerequisites)
3. [Installation](#3-installation)
4. [Understanding ZFS Structure](#4-understanding-zfs-structure)
5. [Pool Management](#5-pool-management)
6. [Dataset Management](#6-dataset-management)
7. [Snapshots](#7-snapshots)
8. [Encryption](#8-encryption)
9. [Scheduled Snapshot Jobs](#9-scheduled-snapshot-jobs)
10. [Scheduled Scrub Jobs](#10-scheduled-scrub-jobs)
11. [Replication Jobs](#11-replication-jobs)
12. [Properties](#12-properties)
13. [Discovery (OMV Sync)](#13-discovery-omv-sync)
14. [Dashboard Widgets and ARC Statistics](#14-dashboard-widgets-and-arc-statistics)
15. [Tips and Best Practices](#15-tips-and-best-practices)

---

## 1. What is ZFS?

ZFS (Zettabyte File System) is a mature, enterprise-grade filesystem and logical volume manager
originally developed by Sun Microsystems for Solaris and now maintained by the OpenZFS project
on Linux. It combines the roles of a traditional filesystem and a volume manager into a single
unified layer, which gives it capabilities that older filesystems like ext4 or XFS cannot match.

### Key capabilities

**Copy-on-write with checksums.** Every block is checksummed when written and verified when
read. If a checksum mismatch is detected (bit rot, failing drive, firmware bug) ZFS knows
exactly which block is corrupt. In a redundant pool it can automatically repair the corruption
from the good copy without any user intervention.

**Snapshots and clones.** A snapshot is a read-only, point-in-time copy of a filesystem. It
consumes no space at creation — space is only used as data in the original filesystem diverges
from the snapshot. Clones are writable copies derived from a snapshot.

**Pooled storage.** Disks are combined into a *pool*. One or more *filesystems* and *volumes*
live inside the pool and share its available space. There are no fixed partition sizes to worry
about.

**Built-in RAID.** RAID is handled by ZFS natively through its VDEV system. No separate hardware
RAID controller is required or recommended.

**Compression.** Transparent, per-dataset compression (LZ4, Zstandard, gzip, etc.) can reduce
storage consumption and often *increase* throughput because less data is written to disk.

**Self-healing.** When a scrub detects corruption in a redundant pool, ZFS restores the damaged
data automatically from the surviving copy.

### Common misconceptions

**RAM requirements.** ZFS does not require large amounts of RAM to function correctly. It will
use available RAM for the Adaptive Replacement Cache (ARC) to speed up reads, but this is a
benefit rather than a burden — the ARC is sized dynamically and returns memory to other processes
when needed. A system with 4–8 GB of RAM running a home NAS workload is perfectly fine.

**ECC memory.** ECC RAM is desirable because it prevents the CPU from writing corrupted data to
disk in the first place. It is not required — ZFS's checksumming still catches errors that reach
storage regardless.

---

## 2. Prerequisites

- **64-bit (amd64) system.** ZFS on Linux is not supported on 32-bit or ARM single-board
  computers such as the Raspberry Pi.
- **OMV-Extras** plugin installed (provides access to additional plugins including this one and
  the Kernel plugin).
- **Proxmox kernel** (strongly recommended). The standard Debian backports kernel can lose ZFS
  module packages across kernel upgrades, causing pools to become inaccessible. The Proxmox
  kernel ships with prebuilt ZFS modules and is updated in lockstep with the ZFS packages,
  eliminating this risk. Install it via the Kernel plugin before installing openmediavault-zfs.

---

## 3. Installation

1. In the OMV web interface, go to **System → Update Management** and apply all pending updates.
2. If you have not already done so, install the **Kernel** plugin from **System → Plugins**, then
   use it to install the Proxmox kernel and reboot.
3. Go to **System → Plugins**, search for **zfs**, and install **openmediavault-zfs**.
4. After installation the **Storage → ZFS** section appears in the navigation.

---

## 4. Understanding ZFS Structure

Before creating anything, it helps to understand how ZFS organises storage.

```
Pool (tank)
├── VDEV 1 — mirror: sda, sdb       ← redundancy lives here
├── VDEV 2 — mirror: sdc, sdd       ← adding a second VDEV expands capacity
├── [log VDEV]                       ← optional sync write accelerator (SLOG)
├── [cache VDEV]                     ← optional read cache (L2ARC)
└── [spare]                          ← optional hot spare

Datasets inside the pool
├── tank                             ← pool root (itself a dataset)
├── tank/documents
├── tank/media
│   ├── tank/media/movies
│   └── tank/media/music
└── tank/vm-disks
    └── tank/vm-disks/win11          ← a zvol (block device), not a filesystem
```

### Pools

A pool is the top-level storage container. It is built from one or more **VDEVs**.

### VDEVs

A VDEV (Virtual Device) is the redundancy unit. The most important rule: **if a VDEV fails
entirely, the pool is lost**. Redundancy protects against individual disk failures *within* a
VDEV, not between VDEVs.

| VDEV type | Redundancy | Min disks | Notes |
|-----------|-----------|-----------|-------|
| Basic (stripe) | None | 1 | Any single disk failure loses the pool. Enable `copies=2` to get self-healing at the cost of halved capacity. |
| Mirror | 1 copy | 2 | All data exists on every disk. Recommended for most home NAS setups. |
| RAIDZ1 | 1 parity | 3 | Tolerates 1 disk loss. Keep to ≤7 disks per VDEV. |
| RAIDZ2 | 2 parity | 4 | Tolerates 2 disk losses. Keep to ≤11 disks per VDEV. |
| RAIDZ3 | 3 parity | 5 | Tolerates 3 disk losses. Keep to ≤15 disks per VDEV. |

A basic VDEV can be converted to mirror, but a mirror VDEV cannot be converted to a RAIDZ VDEV and RAIDZ1 cannot later become a RAIDZ2 VDEV.
Adding more disks to expand a RAIDZ VDEV is possible via RAIDZ expansion (if the pool has
`feature@raidz_expansion` active) but this is a slow, one-at-a-time process.

### Datasets

Datasets are the filesystems and volumes that live inside a pool. There are three types:

- **Filesystem** — a mounted directory tree, like a normal folder.
- **Volume (zvol)** — a block device that appears as a raw disk. Used for virtual machine images,
  iSCSI targets, or anything that needs a block interface.
- **Snapshot** — a read-only point-in-time copy of a filesystem or volume.

Datasets inherit properties from their parent unless overridden. Creating a child dataset
`tank/media/movies` with a different `recordsize` than `tank/media` is valid — only the property
on `tank/media/movies` changes.

### Best practice: never use the pool root directly

Always create at least one named filesystem under the pool and use that for data. The pool root
dataset is typically left with minimal configuration and its mountpoint set to `none` or `/tank`.
This keeps administration cleaner and preserves flexibility for property inheritance.

---

## 5. Pool Management

Navigate to **Storage → ZFS → Pools**.

### Creating a pool

Click **Pool → Add pool**. The form fields are:

| Field | Description |
|-------|-------------|
| **Name** | Pool name. Letters, numbers, hyphens, and underscores. Cannot start with a number. |
| **VDEV type** | Basic, Mirror, RAIDZ1, RAIDZ2, or RAIDZ3. |
| **Devices** | Disks to include. The plugin lists unpartitioned, unused disks. |
| **Device identification** | How disks are identified in the pool: `by-id` (stable across reboots — recommended), `by-path`, or `dev` (e.g. `/dev/sda` — unstable). |
| **Ashift** | Sector size hint. Do NOT rely on auto-detection. Set to `12` for modern 4K HDD/SSD. Only go higher if you know otherwise. |
| **Force** | Pass `-f` to `zpool create`. Required if disks have existing partition tables or ZFS labels. |
| **Compression** | Applied to the pool root dataset and inherited by all children unless overridden. `lz4` is an excellent default — fast and space-efficient. |
| **Mountpoint** | Where the pool root mounts. Defaults to `/<poolname>`. |
| **Case sensitivity** | `sensitive` (Linux default), `insensitive`, or `mixed`. Cannot be changed after creation. |

After the pool is created it appears in the table with its health state, size, free space, and
fragmentation.

### Pool health states

| State | Meaning |
|-------|---------|
| ONLINE | All devices present and functioning normally. |
| DEGRADED | One or more devices have failed but the pool is still readable/writable. Immediate attention required. |
| FAULTED | The pool is completely unavailable. |
| OFFLINE | The pool has been exported or manually taken offline. |
| REMOVED | A device was physically removed. |
| UNAVAIL | A device cannot be opened. |

### Pool actions

Select a pool and use the action menus:

#### Pool menu

| Action | Description |
|--------|-------------|
| **Properties** | View and edit ZFS properties on the pool. |
| **Details** | Raw `zpool status -v` output showing all devices, error counts, and recent events. |
| **History** | Chronological log of all ZFS operations performed on the pool. |
| **Expand pool** | Add a new data VDEV to increase the pool's total capacity. The new VDEV must match the redundancy level you want (adding a mirror VDEV to a mirror pool is common). |
| **Clear errors** | Reset error counters and clear the pool's error log (`zpool clear`). Use after replacing a failed device to confirm the new device is error-free. |
| **Trim** | Send TRIM commands to all SSDs in the pool to reclaim space from deleted data. SSDs only — harmless but ineffective on spinning disks. |
| **Upgrade** | Enable all supported ZFS feature flags. **One-way operation** — the pool cannot be imported by an older ZFS version afterward. Only upgrade when all systems that may need to import this pool are running the same or newer ZFS version. |
| **Fix packages** | Attempt to reinstall or repair corrupted ZFS package installations. |
| **Delete** | Destroy the pool and wipe labels and filesystem signatures from all member disks. **Irreversible.** Deletes all data. |

#### Add VDEV menu

| Action | Description |
|--------|-------------|
| **Cache** | Add an L2ARC (Level 2 Adaptive Replacement Cache) VDEV using one or more SSDs. Extends the read cache beyond RAM. Most beneficial for workloads with large working sets and random reads. |
| **Log** | Add a ZIL (ZFS Intent Log) device using an SSD or NVMe. Accelerates synchronous writes. The plugin automatically mirrors the log if two devices are provided to prevent pool loss on log device failure. |
| **Spare** | Add one or more hot spare disks. If a VDEV member fails, ZFS automatically begins resilver using the spare. |
| **Special** | Add a special allocation class VDEV for metadata and small blocks. Speeds up metadata operations when backed by fast storage (NVMe). The plugin automatically mirrors the special VDEV when two devices are given. |

#### Remove VDEV menu

| Action | Description |
|--------|-------------|
| **Remove vdev** | Initiate removal of a VDEV. ZFS evacuates all data from the VDEV to the remaining pool before it is detached. Only certain VDEV types support removal (not RAIDZ). The pool must have enough free space to hold the evacuated data. |
| **Cancel removal** | Abort an in-progress VDEV removal and restore the VDEV to active use. |
| **Removal status** | View the current progress of a VDEV removal: bytes remaining, estimated time, and status. |

#### Device menu

| Action | Description |
|--------|-------------|
| **Offline** | Temporarily stop using a device without removing it from the pool. The pool remains healthy (DEGRADED) if it has enough redundancy. Use before a planned maintenance swap. |
| **Online** | Bring an offlined device back into service. ZFS resilvered any writes that occurred while the device was offline. |
| **Attach** | Add a device to an existing single-disk (basic) VDEV to convert it into a mirror, or add a disk to an existing mirror to expand it. Add to existing RAIDZ device. |
| **Detach** | Remove one disk from a mirror, leaving the remaining disk(s) in place. The mirror is downgraded accordingly. |
| **Replace** | Swap a failed or unwanted device for a new one. ZFS begins resilvering immediately. The pool remains DEGRADED until resilvering completes. |

#### Scrub menu

| Action | Description |
|--------|-------------|
| **Start scrub** | Begin reading every block in the pool and verifying checksums. Repairs any corruption found using redundant copies. |
| **Stop scrub** | Abort the current scrub immediately. |
| **Pause scrub** | Suspend the scrub. Progress is preserved and the scrub can be resumed later. |
| **Resume scrub** | Continue a paused scrub from where it left off. |
| **Scrub status** | View detailed scrub progress: bytes scanned, errors found, elapsed time, and estimated completion. |

#### Import/Export menu

| Action | Description |
|--------|-------------|
| **Import pool** | Import a pool that is not currently active. Options: import all available pools, import a specific named pool, or force-import a pool that appears to be in use (e.g. after an unclean shutdown on another system). |
| **Export pool** | Flush all pending writes, unmount all datasets, and release the pool so it can be moved to another system or safely powered down. |

### Expanding pool capacity by replacing disks

To replace all disks in a VDEV with larger ones (for example, upgrading from 4 TB to 8 TB drives
in a mirror):

1. Replace one disk at a time using **Device → Replace**. Wait for resilvering to complete
   (`zpool status` shows `scan: resilvered ... with 0 errors`).
2. Repeat for each disk in the VDEV.
3. Once all disks are replaced, the pool will automatically expand to use the full capacity
   because `autoexpand` is enabled by default on new pools.

---

## 6. Dataset Management

Navigate to **Storage → ZFS → Datasets**. The table shows all pools, filesystems, volumes, and
snapshots in a hierarchy. Use the **Type** filter at the top to show only filesystems, only
snapshots, etc.

### Adding a filesystem

Select a pool or parent filesystem and click **Add → Add filesystem**.

| Field | Description |
|-------|-------------|
| **Name** | The dataset name component (not the full path). |
| **Mountpoint** | Where the filesystem mounts. Leave blank to use the default (`/<pool>/<name>`). Set to `none` to create an unmounted dataset. |
| **Compression** | Override the inherited compression algorithm. `inherit` leaves the parent's setting in place. |
| **Record size** | The internal block size for this filesystem. Defaults to 128K (good for general use). Use 4K–16K for databases with small random I/O. Use 1M for media files and sequential workloads. |
| **Case sensitivity** | `sensitive`, `insensitive`, or `mixed`. Set at creation, cannot be changed later. |

After creating a filesystem it can be used as a shared folder target in OMV
(**Storage → Shared Folders**) once registered with **Discover → Add new** (see
[Section 13](#13-discovery-omv-sync)).

### Adding a volume (zvol)

Select a pool or parent filesystem and click **Add → Add volume**.

| Field | Description |
|-------|-------------|
| **Name** | The dataset name component. |
| **Size** | Volume size in binary units (e.g. `10G`, `500M`). |
| **Thin provisioning** | If checked, the zvol is sparse — it does not reserve all its space upfront. The pool reports the full volume size as used but blocks are only actually allocated as data is written. |

Volumes appear as block devices under `/dev/zvol/<pool>/<name>`.

### Adding a snapshot

Select a filesystem or volume and click **Add → Add snapshot**.

| Field | Description |
|-------|-------------|
| **Name** | Snapshot name appended after `@` (e.g. `before-upgrade`). |
| **Recursive** | Also snapshot all child datasets in one atomic operation. |

Alternatively, **Add → Quick snapshot** creates a snapshot with an auto-generated
timestamp name (`<dataset>@<dataset>-YYYYMMDD-HHMMSS`) without prompting.
**Add → Quick recursive snapshot** does the same recursively.

### Cloning a dataset

A clone is a writable filesystem derived from a snapshot.

Select a filesystem and click **Clone**. The plugin automatically creates a snapshot of the
source dataset (named `<source>-clone-base-YYYYMMDD-HHMMSS`) and uses it as the clone origin.
The clone initially shares all data blocks with the source and uses no additional space.

Cloned datasets cannot be encrypted independently — they inherit the encryption root of their
origin. To create an independent encrypted dataset, promote the clone first.

### Promoting a clone

A clone depends on its origin snapshot — the origin cannot be deleted while the clone exists.
**Promote** reverses this relationship: the clone becomes the independent dataset and the origin
becomes dependent on it instead.

Select a clone and click **Promote**. After promotion the original source dataset becomes the
dependent and can be deleted independently.

### Renaming a dataset

Select a filesystem or volume and click **Rename**. The dataset is renamed in ZFS, its mountpoint
is updated, and OMV's fstab database is updated to reflect the new path.

Renaming a dataset that has active shared folders is blocked — remove the shares first.

### Deleting a dataset

Select any dataset and click **Delete**. The plugin checks for dependent clones and active shares
before proceeding. Snapshots of a filesystem are listed and must be removed (or confirmed)
before the parent can be deleted.

For encrypted datasets with auto-unlock configured, always delete through the plugin rather than
via `zfs destroy` on the command line — the plugin cleans up keyfiles, systemd units, and the
ZFS list cache correctly. See the README for manual cleanup steps if needed.

---

## 7. Snapshots

### Snapshot tab

**Storage → ZFS → Snapshots** shows all snapshots across all pools with their size, referenced
data, creation time, and origin.

### Operations on snapshots

Select a snapshot to access:

| Action | Description |
|--------|-------------|
| **Rollback** | Revert the parent dataset to the state at the time of this snapshot. All data written after the snapshot is permanently discarded. The snapshot itself is preserved. |
| **Clone** | Create a writable copy derived from this snapshot. |
| **Diff** | Show which files were created, modified, or deleted between this snapshot and the current state of the dataset (or between two snapshots). Output is presented as a text report. |
| **Delete** | Remove the snapshot and reclaim its space (only space not referenced by other snapshots or the current dataset). |
| **Delete range** | Delete a batch of snapshots relative to the selected one: all snapshots *earlier* than this one, all snapshots *later* than this one, or *all* snapshots on the parent dataset with the same prefix. Useful for bulk cleanup. |

### Recovering files from a snapshot

ZFS snapshots are accessible in the `.zfs/snapshot/<name>/` hidden directory under the
filesystem's mountpoint. For example, if `tank/documents` is mounted at `/tank/documents`, the
snapshot `tank/documents@2024-01-15` is browsable at:

```
/tank/documents/.zfs/snapshot/2024-01-15/
```

No mounting or special tools are needed — simply copy the file back from the snapshot directory.

---

## 8. Encryption

ZFS native encryption protects dataset contents at rest. The encryption key is required to mount
and read a dataset. Without the key the data is unreadable even with physical access to the disks.

### Enabling encryption

Select a pool or parent filesystem and click **Add → Add encrypted filesystem**.

| Field | Description |
|-------|-------------|
| **Name** | Dataset name component. |
| **Encryption algorithm** | AES-128-CCM, AES-192-CCM, AES-256-CCM, AES-128-GCM, AES-192-GCM, or AES-256-GCM. **AES-256-GCM is recommended** — GCM provides authenticated encryption (detects tampering) and is hardware-accelerated on all modern CPUs. |
| **Passphrase** | Must be at least 8 characters. Use a strong, unique passphrase. |
| **Auto-unlock** | Automatically unlock and mount the dataset at boot (stores passphrase in `/etc/zfs/keys/`). |
| **Compression** | Compression algorithm (applied before encryption). |
| **Record size** | Internal block size. |
| **Mountpoint** | Where the dataset mounts. |

> **Note on child datasets:** Child filesystems created under an encrypted dataset inherit the
> encryption and key of their parent. They share the same encryption root and are unlocked and
> locked together with the parent.

### Locking and unlocking

A locked dataset is unmounted and its contents are inaccessible. To lock a dataset, select it
and click **Encryption → Unload key (lock)**. To unlock it, click **Encryption → Load key
(unlock)** and enter the passphrase.

Cloned datasets that are *not* the encryption root cannot be independently locked or unlocked —
use the original encryption root instead.

### Auto-unlock: local keyfile

Local auto-unlock stores the passphrase as a file in `/etc/zfs/keys/` and configures ZFS to
load the key automatically at boot. This protects against drive theft (an attacker who steals
only the disks cannot read them without the server) but does not protect against someone who
steals the entire server.

Select an encrypted dataset and click **Encryption → Enable auto-unlock → Local file**. Enter
the passphrase to confirm. The plugin:

1. Writes the keyfile to `/etc/zfs/keys/<pool_dataset>.key`.
2. Sets `keylocation=file:///etc/zfs/keys/<pool_dataset>.key` on the dataset.
3. Unmasks the `zfs-load-key@<instance>.service` systemd unit so it runs at boot.
4. Writes the dataset path into the ZFS list cache so `zfs-mount-generator` generates the
   correct mount unit.

To remove auto-unlock, click **Encryption → Disable auto-unlock**. The keyfile is deleted,
`keylocation` is reset to `prompt`, and the systemd unit is masked so no passphrase prompt
appears at boot (the dataset simply stays locked until manually unlocked).

### Auto-unlock: remote HTTPS

Remote auto-unlock fetches the encryption key from an HTTPS server at boot. The key is never
stored on the local machine. This protects against both drive theft and full server theft —
an attacker cannot decrypt the data without network access to the key server.

Select an encrypted dataset and click **Encryption → Enable auto-unlock → Remote HTTPS**.

| Field | Description |
|-------|-------------|
| **Key URL** | Full HTTPS URL where the key material is served. Must begin with `https://`. |
| **Skip certificate verification** | Disable TLS certificate validation. Use only with self-signed certificates on a trusted network. |
| **Decryption passphrase** | If the key file at the URL is OpenSSL-encrypted, provide the passphrase to decrypt it. Leave blank if the URL serves the raw key directly. |
| **Network timeout** | How long (in seconds) to wait for the key server before giving up. Range 10–600, default 60. Allow enough time for the network interface to come up. |

The `zfs-remote-unlock` service runs at boot, fetches the key, and loads it before the ZFS
mount units start.

### Changing the encryption key

Select an encrypted dataset and click **Encryption → Change encryption key**. Enter the current
passphrase and the new passphrase. The key change applies to the encryption root; all child
datasets sharing the root are automatically covered.

---

## 9. Scheduled Snapshot Jobs

Navigate to **Storage → ZFS → Snapshot Jobs**.

Snapshot jobs run `omv-zfs-snapshot` on a schedule to create snapshots automatically and prune
old ones according to your retention policy.

### Creating a snapshot job

Click **Add** to open the form.

#### Settings

| Field | Description |
|-------|-------------|
| **Enabled** | Toggle the job on or off without deleting it. |
| **Dataset** | The dataset to snapshot. |
| **Snapshot prefix** | A label prepended to every snapshot name. Pattern: alphanumeric, hyphens, underscores. Snapshots are named `<prefix>-YYYYMMDD-HHMMSS`. Default: `auto`. The prefix must be unique per dataset if multiple jobs target the same dataset. |
| **Keep** | How many or how long to retain snapshots (see below). |
| **Retention unit** | `Snapshots` (count), `Days`, `Weeks`, `Months`, or `Years`. |
| **Recursive** | Snapshot this dataset and all its children in one atomic operation. Retention counting is done at the parent level only — child snapshots with the same name are pruned when the parent generation is pruned. |

#### Retention behaviour

- **Count mode:** After creating the new snapshot, count all snapshots whose names match the
  prefix. Delete the oldest ones until only `Keep` remain.
- **Time mode:** Delete any snapshot whose embedded timestamp is older than `Keep` units ago.
  For example, `Keep=30, Days` deletes snapshots older than 30 days.

#### Schedule

| Field | Description |
|-------|-------------|
| **Time of execution** | `Exactly` (cron expression), `Hourly`, `Daily`, `Weekly`, `Monthly`, `Yearly`, or `At reboot`. |
| **Minute/Hour/Day/Month/Day of week** | Available when `Exactly` is selected. Accepts `*` or a comma-separated list of values. The "Every N" checkboxes convert a single value into a `*/N` cron expression. |

#### Notifications

| Field | Description |
|-------|-------------|
| **Send output via email** | Email the job output (stdout + stderr) to root on completion. Requires a working mail relay configured in OMV (**System → Notification**). |
| **Send email on error only** | Only send an email if the job exits with a non-zero status. Suppresses routine success emails. |
| **Comment** | Appended to the email subject line to help identify the job. |

### Running a job manually

Select a job in the table and click **Run**. The job executes immediately in the background.
Check **Storage → ZFS → Logs → ZFS Snapshot** to see the output.

---

## 10. Scheduled Scrub Jobs

Navigate to **Storage → ZFS → Scrub Jobs**.

Scrub jobs run `zpool scrub` on a schedule to periodically verify pool integrity. The scrub
reads every block, checks its checksum, and repairs any corruption it finds using redundant
copies. Regular scrubbing is the primary mechanism for detecting and correcting silent
corruption before it spreads.

> **Important:** A scrub job *starts* the scrub and exits immediately. The actual scrub runs
> asynchronously in the background and may take hours on a large pool. Monitor progress via
> **Pool → Scrub → Scrub status** or `zpool status` in the shell.

### Creating a scrub job

| Field | Description |
|-------|-------------|
| **Enabled** | Toggle the job on or off. |
| **Pool** | The pool to scrub. |
| **Schedule** | Same scheduling options as snapshot jobs. Default: Weekly. |
| **Send output via email** | Email a brief start notification to root. |
| **Send email on error only** | Only email if the scrub fails to start. |
| **Comment** | Appended to the email subject. |

**Recommended schedule:** Monthly is sufficient for healthy pools on consumer hardware. Weekly
is appropriate for pools under heavy write load. Scrubbing is I/O-intensive — schedule it during
off-peak hours.

---

## 11. Replication Jobs

Navigate to **Storage → ZFS → Replication Jobs**.

Replication jobs copy a ZFS dataset (and its snapshots) to another location using `zfs send` and
`zfs receive`. Subsequent runs send only the incremental difference since the last replication,
making them efficient even for large datasets. Replication can target a local pool or a remote
host over SSH.

### Creating a replication job

#### Source

| Field | Description |
|-------|-------------|
| **Enabled** | Toggle the job on or off. |
| **Source dataset** | The dataset to replicate. |
| **Snapshot prefix** | Prefix for replication snapshots. Default: `replicate`. If using **Use existing snapshots**, this must exactly match the prefix used by the snapshot job. |
| **Recursive** | Replicate the dataset and all its children. |
| **Raw send** | Use `zfs send -w` to send encrypted data without decrypting it first. The destination receives the data in its encrypted form. Requires the source dataset to be encrypted. The destination does not need to have the key. |
| **Use existing snapshots** | Instead of creating a new snapshot, find the most recent snapshot matching the prefix and send it. Recommended when a separate snapshot job is already managing snapshots for this dataset. Prevents two jobs from fighting over snapshot creation and avoids breaking the incremental chain. |
| **Keep snapshots on source** | Number of replication snapshots to retain on the source after a successful send. `0` keeps all snapshots. Ignored when **Use existing snapshots** is enabled (the snapshot job controls retention instead). |
| **Use ZFS bookmarks** | After each successful send, create a ZFS bookmark as the incremental base for the next run. Bookmarks survive snapshot deletion, so you can aggressively prune the source while still sending incremental data. **Cannot be used with Recursive mode** (ZFS limitation). |

#### Destination

| Field | Description |
|-------|-------------|
| **Type** | `Local` (same machine, different pool) or `Remote` (SSH). |
| **Destination dataset** | Path on the destination where data is received (e.g. `backup/replica`). The dataset is created on the first run. |
| **Remote host** | Hostname or IP address of the destination server. Remote only. |
| **SSH Certificate** | The SSH key pair to authenticate with. Create one under **System → Certificates → SSH** and copy it to the remote host before creating the replication job. |
| **SSH user** | Remote user account. Default: `root`. Non-root users need `zfs allow` delegation on the destination. |
| **SSH port** | SSH port on the destination. Default: 22. |

#### Schedule and Notifications

Same scheduling and email options as snapshot and scrub jobs.

### Replication strategies

**Scenario: daily replication with independent snapshot retention**

This is the most common setup. A snapshot job manages snapshots on the source; the replication
job sends them to the destination without creating its own.

1. Create a snapshot job for `tank/documents` with prefix `daily`, execution `Daily`, keep `30
   Days`.
2. Create a replication job for `tank/documents` with prefix `daily`, execution `Daily`,
   **Use existing snapshots** enabled, **Keep snapshots on source** = `0`, destination
   `backup/documents` on a local backup pool.

The snapshot job runs first (or within the same cron minute), then the replication job sends
the newest `daily-*` snapshot incrementally.

**Scenario: self-contained replication with source pruning**

When no separate snapshot job exists:

1. Create a replication job for `tank/vm-disks` with prefix `replicate`, keep `3` on source,
   destination on a remote server.

The replication job creates its own snapshot, sends it, then prunes old replication snapshots
on the source to keep only 3.

**Scenario: efficient replication with bookmarks**

Suitable when disk space on the source is limited:

1. Create a snapshot job for `tank/photos` with prefix `weekly`, keep `1 Snapshots`.
2. Create a replication job for `tank/photos` with prefix `weekly`, **Use existing snapshots**
   enabled, **Use ZFS bookmarks** enabled.

The replication job uses the bookmark from the previous run as its incremental base, so the
single retained snapshot is sufficient — there is no need to keep an older snapshot just for
the next send.

### Remote replication: SSH setup

Before creating a remote replication job:

1. In OMV, go to **System → Certificates → SSH** and generate a new SSH key pair.
2. Click **Copy to Remote** for that certificate, entering the destination host, user, and
   password. This copies the public key to `~/.ssh/authorized_keys` on the remote host.
3. In the replication job form, select that certificate under **SSH Certificate**.

The replication script uses strict host key checking — the first connection to a new host is
accepted automatically, but subsequent connections reject unknown key changes.

### Replication logs

Job output is written to `/var/log/omv-zfs-replicate.log` and is also viewable in the OMV
web interface under **Storage → ZFS → Logs → ZFS Replicate**.

---

## 12. Properties

ZFS properties control the behaviour of pools and datasets. They can be inherited from a parent
(shown as source `inherited`) or set explicitly on a dataset (source `local`).

### Viewing and editing properties

Select any pool, filesystem, volume, or snapshot and click **Properties**. The table shows every
property, its current value, its source, and whether it can be edited. Click the edit icon on
a row to change a property's value.

Custom user-defined properties (in `namespace:name` format) can also be added.

### Frequently adjusted properties

| Property | Values | Notes |
|----------|--------|-------|
| `compression` | `off`, `lz4`, `zstd`, `gzip`, `lzjb`, `zle` | `lz4` is fast and effective for most data. `zstd` offers better compression ratios for archival data at some CPU cost. Already-compressed files (video, ZIP, JPEG) do not compress further. |
| `recordsize` | `4K`–`1M` | The internal block size. Larger values suit sequential workloads; smaller values suit databases with random I/O. Default 128K. |
| `atime` | `on`, `off` | Whether to update access time when files are read. `off` reduces write amplification significantly on read-heavy datasets. |
| `copies` | `1`, `2`, `3` | Number of copies of each block. Setting `copies=2` on a non-redundant (basic) VDEV gives self-healing capability at the cost of halved usable capacity. |
| `quota` | e.g. `100G` | Maximum space this dataset and all its children can use. |
| `refquota` | e.g. `100G` | Maximum space this dataset alone can use (excluding children). |
| `reservation` | e.g. `50G` | Guaranteed space reserved for this dataset and its children. |
| `dedup` | `on`, `off` | Block-level deduplication. Requires roughly 5 GB of RAM per TB of data for the dedup table. Generally not recommended for home NAS unless data is extremely repetitive. |
| `sync` | `standard`, `always`, `disabled` | Controls when writes are committed to disk. `always` is safest but slowest. `disabled` is fastest but risks data loss on power failure. |
| `snapdir` | `hidden`, `visible` | Whether `.zfs/snapshot/` is visible in directory listings. `visible` makes snapshot access easier for users. |
| `readonly` | `on`, `off` | Prevent writes to the dataset. |

### Raw details

Select a dataset and click **Details** to see the raw output of `zpool status -v` (for pools) or
`zfs get all` (for filesystems, volumes, and snapshots). This is useful for debugging or checking
the exact value of any property without the filtered view.

---

## 13. Discovery (OMV Sync)

ZFS filesystems must be registered in OMV's internal database before they can be used as shared
folder targets. **Discovery** synchronises OMV's fstab database with the live ZFS state.

This is accessible from both the Pools table and the Datasets table under **Discover**.

| Action | Description |
|--------|-------------|
| **Add new** | Find any ZFS filesystems that exist in ZFS but are not yet in OMV's database, and register them. Use this after creating a new dataset that you want to share. |
| **Add new + delete missing** | Full bidirectional sync: adds unregistered datasets *and* removes stale entries for datasets that no longer exist. |
| **Delete missing** | Remove OMV fstab entries for datasets that have been deleted from ZFS but whose entries still remain in the database. |

> **Note:** The plugin backs up `config.xml` before every discovery operation. The 20 most
> recent backups are kept at `/etc/openmediavault/config.xml.<timestamp>`.

> **Locked encrypted datasets** are never removed by **Delete missing** — the plugin recognises
> that a dataset may simply be locked rather than deleted.

---

## 14. Dashboard Widgets and ARC Statistics

### Dashboard widgets

The plugin adds four dashboard widgets. Enable them via **Dashboard → Configure widgets**
(the gear icon on the dashboard).

| Widget | Description |
|--------|-------------|
| **Pool Health** | A table showing the health status of every pool (ONLINE, DEGRADED, FAULTED, etc.). |
| **Pool Status** | A compact table showing each pool's health, capacity percentage, and used/available space. |
| **ARC Size** | A single number showing the current total ARC (Adaptive Replacement Cache) size. |
| **ZFS ARC Hit/Misses** | The ratio of total hits to misses. A hit ratio above 90% indicates the cache is effective for your workload. |

### ARC Statistics page

Navigate to **Storage → ZFS → ARC Stats** for a full text dump of ARC internals: total size,
MRU/MFU cache sizes, hit/miss counters, prefetch statistics, and more. This is the same output
as the `arc_summary`/`zarcsummary` command-line tool.

### ZFS Logs

Four log viewers are available under **Storage → ZFS → Logs**:

| Log | Contents |
|-----|---------|
| **ZFS Events** | Output from the ZFS Event Daemon (ZED). Includes pool state changes, device errors, scrub completions, and resilver events. Monitor this for health alerts. |
| **ZFS Snapshot** | Output from all snapshot job runs (`/var/log/omv-zfs-snapshot.log`). |
| **ZFS Scrub** | Output from all scrub job runs (`/var/log/omv-zfs-scrub.log`). |
| **ZFS Replicate** | Output from all replication job runs (`/var/log/omv-zfs-replicate.log`). |

---

## 15. Tips and Best Practices

### Pool design

Designing a ZFS pool layout is about balancing **performance, redundancy, capacity, and future flexibility**. Decisions made up front may be hard to change later, so plan carefully.

**The optimum pool layout depends on how the data will be used.** For sequential workloads (media, backups, archive) RAIDZ works well, but prefer RAIDZ2 for larger drives to reduce resilver second-failure risk. For random I/O (containers, VMs, databases) use mirrors — they offer the best combination of read performance (spread across disks), write performance, and faster resilver after disk replacement.

**Do not add optional devices (log, cache, and/or special VDEV) without justification and understanding.** For example, a "log device" is useless on a home NAS which does not generate sync writes (SMB shares do not, NFS shares do). Analyse first, but the answer to improving pool performance may simply be to add RAM.

**For a mix of SSDs and HDDs, consider using the SSDs in a mirror pool and the HDDs in a separate RAIDZ pool.** In a home NAS this separation is preferable to trying to enhance RAIDZ performance by using a log, cache, and/or special VDEV, which only adds complexity and cost (typical consumer SSD/NVMe devices are not suitable for these optional VDEVs).

**Beware adding a basic VDEV to an existing mirror/RAIDZ pool.** This turns a pool into a stripe, where losing a single drive can lose the entire pool.

- **Use `by-id` device identification.** Device names like `/dev/sda` can change after a
  reboot if drives are added or removed. Stable `by-id` paths ensure ZFS always finds the
  right disk.
- **Enable compression on all datasets.** `lz4` is essentially free — it is so fast that
  on most systems it *improves* throughput even for data that compresses modestly.

### Dataset organisation

- **Create a filesystem per use case** rather than dumping everything into the pool root.
  Separate datasets for documents, media, backups, and VM images let you tune `recordsize`,
  `compression`, and quotas independently.
- **Set `recordsize=1M` for media libraries** (movies, music, photos). Sequential reads of
  large files benefit enormously from large record sizes.
- **Set `recordsize=4K` or `8K` for database files** (PostgreSQL, MySQL, SQLite). These
  databases do their own internal block management and need ZFS's blocks to match.
- **Disable `atime` on busy datasets.** Every file read normally triggers a metadata write to
  update the access time. `atime=off` eliminates this overhead.

### Snapshots

- **Automate snapshots with scheduled jobs** rather than relying on manual creation. A daily
  snapshot job with 30-day retention costs almost nothing in disk space for typical NAS data
  and provides a safety net against accidental deletion.
- **Name snapshot prefixes clearly.** Use `daily`, `weekly`, `monthly` rather than `auto` when
  running multiple snapshot jobs per dataset at different frequencies.
- **Do not delete the most recent common snapshot before replication.** The replication script
  needs at least one snapshot that exists on both source and destination to perform an
  incremental send. Deleting it forces a full send.

### Scrubbing

- **Schedule monthly scrubs** at minimum. On systems with heavy write loads or older disks,
  weekly is better. Scrubs are the only way to find silent corruption before it becomes
  unrecoverable.
- **Check scrub results** in the ZFS Events log or via `zpool status` after each scrub.
  Any errors reported warrant investigation.

### Encryption

- **Choose AES-256-GCM.** It is the strongest available option and is hardware-accelerated
  on all x86-64 CPUs with AES-NI (essentially all modern systems).
- **Keep a copy of your passphrase offline.** A lost passphrase means permanently lost data.
  Store it in a password manager and print a copy kept in a physically secure location.
- **Use remote HTTPS auto-unlock for maximum security.** Local keyfile auto-unlock is
  convenient but an attacker who steals the whole server gets the key with the disks. A remote
  key server eliminates this risk.
- **Always delete encrypted datasets through the plugin**, not via `zfs destroy`. The plugin
  cleans up keyfiles, systemd units, and the ZFS list cache. A bare `zfs destroy` leaves
  orphaned artifacts.

### Replication

- **Test your replication jobs** by running them manually and verifying the destination has
  the expected datasets. Check the replication log for errors.
- **Use a separate snapshot job with `Use existing snapshots`** rather than letting the
  replication job manage its own snapshots. This gives you independent control over local
  snapshot retention and avoids conflicts if you run multiple replication jobs for the same
  dataset.
- **Remote replication requires a working SSH trust relationship.** Use OMV's
  **System → Certificates → SSH → Copy to Remote** to set this up before creating the job.
