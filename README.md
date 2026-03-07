# openmediavault-zfs

A plugin for [OpenMediaVault](https://www.openmediavault.org/) that provides ZFS pool, filesystem, volume, and snapshot management through the OMV web interface.

## Features

- **Pool management** — create, expand, import, export, and delete ZFS pools with support for basic, mirror, RAIDZ1, RAIDZ2, and RAIDZ3 topologies
- **Filesystems** — create and delete ZFS filesystems with optional custom mountpoints; nested filesystems supported
- **Volumes (zvols)** — create thick or thin-provisioned block device volumes
- **Snapshots** — create, roll back, and delete snapshots; clone filesystems from snapshots
- **Properties** — view and modify ZFS properties on pools, filesystems, volumes, and snapshots
- **Scrub** — initiate pool integrity scrubs from the UI
- **Discover** — synchronise the OMV fstab database with the live ZFS state:
  - *Add new* — register any ZFS datasets not yet known to OMV
  - *Add new + delete missing* — full sync in both directions
  - *Delete missing* — remove OMV fstab entries for datasets that no longer exist
- **ARC statistics** — dashboard widget showing ZFS ARC hit ratio and cache size
- **Encryption** — enable, load, unload, and change encryption keys on datasets

## ZFS topology reference

```
<zpool>   ::= <vdev> [<log>] [<cache>] [<spare>]
<vdev>    ::= <basic> | <mirror> | <raidz1> | <raidz2> | <raidz3>
<basic>   ::= "disk"
<mirror>  ::= "disk" "disk" ["disk" ...]           (≥ 2 disks)
<raidz1>  ::= "disk" "disk" "disk" ["disk" ...]    (≥ 3 disks)
<raidz2>  ::= "disk" "disk" "disk" "disk" [...]    (≥ 4 disks)
<raidz3>  ::= "disk" "disk" "disk" "disk" "disk" [...] (≥ 5 disks)
<log>     ::= "internal" | <basic> | <mirror>
<cache>   ::= "internal" | <basic>
<spare>   ::= <basic> ["disk" ...]
```

## Integration tests

`tests/test-rpc.sh` is an end-to-end integration test that exercises the plugin's RPC methods against a real ZFS pool.

### Requirements

- Run as root
- One or more block devices that can be **wiped** (use loop devices, LVM logical volumes, or spare disks)
- `python3` available on PATH (standard on all OMV installations)

### Usage

```bash
sudo tests/test-rpc.sh /dev/sdX [/dev/sdY ...]
```

The pool topology is chosen automatically based on how many devices are supplied:

| Devices | Pool type |
|---------|-----------|
| 1       | basic     |
| 2       | mirror    |
| 3–4     | raidz1    |
| 5+      | raidz2    |

The pool and all datasets are destroyed on exit regardless of whether the tests pass or fail.

### Test coverage

#### Informational RPCs
Calls that require no pool and verify the plugin can communicate with the engine:
- `getStats` — reads ARC hit/miss counters and cache size from `/proc`
- `listCompressionTypes` — returns the list of available compression algorithms
- `getEmptyCandidates` — returns unused, unpartitioned block devices

#### Pool — create
- `addPool` — creates a pool on the supplied devices using the selected topology; verified independently with `zpool list`

#### Pool — list, details, properties
- `listPools` / `listPoolsBg` — enumerate pools and their status, size, and mountpoint; the background variant is also exercised
- `getObjectDetails` — retrieves raw `zpool status` and `zpool get all` output for the pool
- `getProperties` — reads all ZFS filesystem properties (compression, quota, atime, etc.) on the pool root dataset
- `setProperties` — sets `compression=lz4` on the pool root dataset via `zfs set`
- `scrubPool` — starts a pool scrub

#### Filesystem — add, details, properties
- `addObject (filesystem)` — creates `fs1` with the default mountpoint
- `addObject (filesystem, custom mountpoint)` — creates `fs2` at a custom path
- `addObject (nested filesystem)` — creates `fs1/child` to verify nested dataset creation
- `getObjectDetails` — reads `zfs get all` for a filesystem
- `getProperties` — reads all properties for a filesystem, verified to include `compression`
- `setProperties` — sets `compression=lz4` and `atime=off` on the filesystem

#### Snapshot — add, list, rollback, delete
- `addObject (snapshot)` — snapshots `fs1` as `fs1@snap1`
- `getAllSnapshots` / `getAllSnapshotsBg` — lists all snapshots; verified to include the new snapshot
- `getObjectDetails` — reads `zfs get all` for the snapshot
- `rollbackSnapshot` — rolls back `fs1` to `snap1`; a marker file is written before the snapshot and its absence after rollback is verified
- `deleteObject (snapshot)` — removes `fs1@snap1`

#### Clone — create and delete
- `addObject (snapshot)` — creates `fs1@snap2` as the clone source
- `addObject (clone)` — clones `fs1@snap2` into `clone1`
- `deleteObject (clone)` — destroys the clone filesystem
- `deleteObject (snapshot)` — removes `fs1@snap2`

#### Volume — thick and thin
- `addObject (volume, thick)` — creates a 100 MiB thick-provisioned zvol
- `addObject (volume, thin)` — creates a 100 MiB thin-provisioned (sparse) zvol
- `getObjectDetails` — verifies `volsize` appears in the output
- `getProperties` — reads zvol properties
- `deleteObject` ×2 — removes both volumes

#### Discover — add new
- A filesystem is created directly via the ZFS CLI, bypassing the plugin
- `doDiscoverBg (addMissing=true)` — the plugin scans for unregistered datasets and adds the missing fstab entry
- The OMV FsTab database is queried to confirm the entry was created

#### Discover — delete stale
- The CLI-created filesystem from the previous step is destroyed via the ZFS CLI, leaving a stale OMV fstab entry behind
- `doDiscoverBg (deleteStale=true)` — the plugin removes fstab entries for datasets that no longer exist
- The OMV FsTab database is queried to confirm the entry was removed

#### Discover — add new with stale entries present
- A filesystem is registered via the plugin and then destroyed via the CLI, creating a stale entry
- `doDiscoverBg (addMissing=true, deleteStale=false)` — verifies the operation completes without error even when stale entries are present (regression test for a bug where `zfs get` was called on non-existent datasets)
- The stale entry is cleaned up with a subsequent `deleteStale` call

#### Export and import
- Child datasets are removed to allow a clean export
- `exportPool` — exports the pool; `listPools` is queried to confirm the pool is no longer visible
- `importPool` — imports the pool by name; `listPools` is queried to confirm it is visible again

#### Pool — delete
- `deleteObjectBg (Pool)` — destroys the pool; verified with `zpool list`
