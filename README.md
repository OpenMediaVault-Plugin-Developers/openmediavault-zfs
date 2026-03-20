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

#### Encryption — unload key with busy child dataset
- An encrypted parent dataset and a mounted child are created
- `unloadEncryptionKey` — verifies `-r` is used on both `zfs unmount` and `zfs unload-key` so the call succeeds even when child datasets are mounted

#### Encryption — auto-unlock lifecycle
- `enableEncryption` — creates an encrypted dataset with `autounlock=true`; keyfile is written to `/etc/zfs/keys/` and `keylocation` is set to `file://`
- `addObject (filesystem, child of encrypted parent)` — verifies the child is immediately mounted (CASE 2 regression)
- `getEncryptionStatus` — confirms `autounlock=true` and `keystatus=available`
- `removeAutoUnlock` — disables auto-unlock: keyfile removed, `keylocation` reset to `prompt`
  - Verifies the `zfs-load-key@<instance>.service` unit is **masked** (a `/dev/null` symlink is written to `/etc/systemd/system/`) so `zfs-mount-generator` does not prompt for a passphrase at the next boot
  - Verifies the `wait-for-remote-unlock.conf` drop-in state matches the number of datasets with active remote unlock (absent when none; present when others exist on the system)
- `setAutoUnlock` (re-enable local) — re-creates the keyfile and **unmasks** the `zfs-load-key@<instance>.service` unit so auto-unlock is active again at the next boot; keyfile presence verified
- `removeAutoUnlock` (second call) — re-masks the unit before the simulated-reboot portion of the test
- `unloadEncryptionKey` — simulates a post-reboot locked state (`keystatus=unavailable`)
- `doDiscoverBg (deleteStale=true)` — verifies the OMV fstab entry is **preserved** for a locked (unmounted) encrypted dataset (regression: a naive stale-entry scan would incorrectly delete it)
- `loadEncryptionKey` — manually unlocks and mounts the dataset after the simulated reboot; parent and child datasets verified as mounted
  - The RPC temporarily unmasks any masked `zfs-load-key@` units before starting them, so the associated `.mount` units can satisfy their `BindsTo` dependencies — without this, systemd unmounts child datasets immediately after `zfs mount` succeeds because the parent `.mount` unit is in a failed state; units are re-masked after mounting to preserve the `removeAutoUnlock` setting

#### Encryption — clone and promote edge cases
- `cloneDataset` — clones an encrypted dataset; `isencryptionroot` verified (source=true, clone=false)
- `unloadEncryptionKey` / `loadEncryptionKey` — rejected with a clear error when called on a non-encryptionroot clone; the real encryption root is unaffected
- `promoteClone` — promotes the clone to become the new encryption root; `isencryptionroot` re-verified on both datasets

#### Encryption — zfs-list.cache and deleteObject cleanup
- An encrypted dataset with `autounlock=true` is created so the cache has a `keylocation=file://` entry and `zfs-mount-generator` generates a `zfs-load-key@` unit for it
- `deleteObject (Filesystem)` — destroys the dataset; verifies the entry is removed from `/etc/zfs/zfs-list.cache/<pool>`, the keyfile is removed from `/etc/zfs/keys/`, and the `zfs-load-key@<instance>.service` unit is no longer active and is unmasked (removing any stale `/dev/null` symlink left by a prior `removeAutoUnlock` call)

#### Encryption — stale mountpoint directory repair
- An encrypted dataset is created; its mountpoint directory is manually removed to simulate corruption
- `loadEncryptionKey` — verifies `addOMVMntEntForDataset` is triggered to recreate the missing mountpoint directory before mounting

#### Encryption — non-mountable datasets in hierarchy
- Datasets with `canmount=off` and `mountpoint=none` are created as children of an encrypted root
- `loadEncryptionKey` — verifies the call succeeds and reports mounted for the normal child, while correctly leaving non-mountable children unmounted (regression: previously required all sharing datasets to reach `mounted=yes`)

#### Encryption — sub-encryption-root in hierarchy (CASE 1)
- A nested hierarchy with an inner dataset that is its own encryption root (key not loaded) is constructed
- `loadEncryptionKey` on the outer root — verifies mountable children are mounted while the inner sub-encryption-root (unavailable key) is correctly left unmounted without causing the outer call to fail

## Deleting an encrypted dataset from the command line

If a dataset has auto-unlock (local keyfile or remote URL) configured in the plugin, deleting it with `zfs destroy` directly leaves orphaned plugin artifacts: a keyfile or remote-key JSON file, a masked `zfs-load-key@` systemd unit, and possibly the `wait-for-remote-unlock.conf` drop-in. Use the RPCs from the command line to let the plugin clean up correctly.

### Using the plugin RPCs (recommended)

Replace `pool/dataset` with your actual dataset path.

```bash
# 1. Remove auto-unlock (deletes keyfile/metadata, unmasks the load-key unit,
#    removes the drop-in if no other datasets have remote unlock, daemon-reload).
omv-rpc -u admin "Zfs" "removeAutoUnlock" '{"name":"pool/dataset"}'

# 2. If the dataset is locked (keystatus=unavailable), load the key manually
#    so zfs destroy can unmount it.  Skip this step if already unlocked.
omv-rpc -u admin "Zfs" "loadEncryptionKey" '{"name":"pool/dataset","key":"your-passphrase"}'

# 3. Destroy the dataset (and any children).  The plugin removes the
#    zfs-list.cache entry, stops the load-key unit, and runs daemon-reload.
omv-rpc -u admin "Zfs" "deleteObject" \
    '{"name":"pool/dataset","mp":"/pool/dataset","type":"Filesystem"}'
```

### Manual cleanup (if zfs destroy was already run)

If you already destroyed the dataset with `zfs destroy` and need to clean up:

```bash
DATASET="pool/dataset"
SAFE="${DATASET//\//_}"          # pool/dataset → pool_dataset

# Remove local keyfile (local auto-unlock).
rm -f "/etc/zfs/keys/${SAFE}.key"

# Remove remote-key metadata (remote auto-unlock).
rm -f "/etc/zfs/remote-keys/${SAFE}.json"

# Unmask the zfs-load-key@ unit that removeAutoUnlock would have masked.
systemctl unmask "zfs-load-key@$(systemd-escape -p "$DATASET").service"

# If no remote-key JSON files remain, remove the ordering drop-in and disable
# the remote-unlock service.
if ! ls /etc/zfs/remote-keys/*.json 2>/dev/null | grep -q .; then
    rm -f /etc/systemd/system/zfs-load-key@.service.d/wait-for-remote-unlock.conf
    systemctl disable --now zfs-remote-unlock.service 2>/dev/null || true
fi

systemctl daemon-reload

# Remove any stale OMV fstab entry for the destroyed dataset.
omv-rpc -u admin "Zfs" "doDiscover" '{"addMissing":false,"deleteStale":true}'
```
