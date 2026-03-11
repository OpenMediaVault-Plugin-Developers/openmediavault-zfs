#!/usr/bin/env bash
# test-rpc.sh — Integration tests for openmediavault-zfs RPC methods.
#
# Usage: sudo ./tests/test-rpc.sh /dev/sdX [/dev/sdY ...]
#
# Creates a temporary ZFS pool on the given device(s), exercises the plugin
# RPC methods, then destroys everything on exit.
#
# WARNING: All supplied devices will be wiped.

set -uo pipefail

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
if [ $# -lt 1 ]; then
    echo "Usage: $(basename "$0") /dev/sdX [/dev/sdY ...]" >&2
    exit 1
fi
if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root." >&2
    exit 1
fi

DEVICES=("$@")
POOL="omvzfstest$$"

# ---------------------------------------------------------------------------
# Colours / counters
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

PASS=0
FAIL=0
declare -a FAILED_TESTS=()

section() { echo -e "\n${CYAN}${BOLD}=== $* ===${NC}"; }
info()    { echo -e "  ${YELLOW}»${NC} $*"; }

_pass() {
    echo -e "  ${GREEN}PASS${NC}  $1"
    ((PASS++)) || true
}
_fail() {
    echo -e "  ${RED}FAIL${NC}  $1"
    [ -n "${2:-}" ] && echo -e "         ${RED}→${NC} $2"
    ((FAIL++)) || true
    FAILED_TESTS+=("$1")
}

# ---------------------------------------------------------------------------
# RPC helpers
# ---------------------------------------------------------------------------

# Call an RPC; return output on stdout, exit non-zero on error.
rpc() {
    local svc=$1 method=$2 params=${3:-'{}'}
    omv-rpc -u admin "$svc" "$method" "$params"
}

# Assert an RPC call succeeds.  Optional 4th arg: grep pattern that must appear
# in the output.  Prints output on success (useful for capturing).
assert_rpc() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'} pattern=${5:-}
    local out ec=0
    out=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -ne 0 ]; then
        _fail "$desc" "$(echo "$out" | tail -3)"
        return 1
    fi
    if [ -n "$pattern" ] && ! echo "$out" | grep -q "$pattern"; then
        _fail "$desc" "Pattern '$pattern' not found in output: ${out:0:200}"
        return 1
    fi
    _pass "$desc"
    echo "$out"
    return 0
}

# Assert an RPC call fails (non-zero exit or output contains Exception).
assert_rpc_fails() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'}
    local out ec=0
    out=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -eq 0 ] && ! echo "$out" | grep -qi "exception"; then
        _fail "$desc" "Expected failure but RPC succeeded"
        return 1
    fi
    _pass "$desc"
    return 0
}

# Call a *Bg method, wait for the background task, report result.
assert_rpc_bg() {
    local desc=$1 svc=$2 method=$3 params=${4:-'{}'}
    local filename ec=0
    filename=$(omv-rpc -u admin "$svc" "$method" "$params" 2>&1) || ec=$?
    if [ $ec -ne 0 ]; then
        _fail "$desc" "Failed to start bg task: ${filename:0:200}"
        return 1
    fi
    filename=$(echo "$filename" | tr -d '"')

    # Poll until the task is no longer running.
    local timeout=120 elapsed=0
    while [ $elapsed -lt $timeout ]; do
        local running
        running=$(omv-rpc -u admin "Exec" "isRunning" \
            "{\"filename\":\"$filename\"}" 2>/dev/null || echo "false")
        echo "$running" | grep -q "true" || break
        sleep 2; ((elapsed += 2)) || true
    done
    if [ $elapsed -ge $timeout ]; then
        _fail "$desc" "Bg task timed out after ${timeout}s"
        return 1
    fi

    # Retrieve and inspect the task output.
    local content
    content=$(omv-rpc -u admin "Exec" "getOutput" \
        "{\"filename\":\"$filename\",\"pos\":0}" 2>/dev/null \
        | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('output',''))" \
        2>/dev/null || echo "")
    if echo "$content" | grep -q "Exception"; then
        _fail "$desc" "$(echo "$content" | grep "Exception" | head -2)"
        return 1
    fi

    _pass "$desc"
    return 0
}

# Check whether the OMV FsTab database contains an entry with the given fsname.
mntent_exists() {
    omv-rpc -u admin "FsTab" "enumerateEntries" '{}' 2>/dev/null \
        | python3 -c "
import sys, json
entries = json.load(sys.stdin)
print(any(e.get('fsname') == '$1' for e in entries))
" 2>/dev/null || echo "False"
}

# ---------------------------------------------------------------------------
# Cleanup — always runs on exit
# ---------------------------------------------------------------------------
cleanup() {
    section "Cleanup"
    info "Destroying pool $POOL (if it exists)"
    zpool destroy -f "$POOL" 2>/dev/null || true
    info "Clearing device labels"
    for dev in "${DEVICES[@]}"; do
        zpool labelclear -f "$dev" 2>/dev/null || true
    done
    info "Removing any remaining mntent entries for this pool"
    omv-rpc -u admin "Zfs" "doDiscover" \
        '{"addMissing":false,"deleteStale":true}' 2>/dev/null || true
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Determine pool type from device count
# ---------------------------------------------------------------------------
DEVCOUNT=${#DEVICES[@]}
if   [ "$DEVCOUNT" -ge 5 ]; then POOLTYPE="raidz2"
elif [ "$DEVCOUNT" -ge 3 ]; then POOLTYPE="raidz1"
elif [ "$DEVCOUNT" -ge 2 ]; then POOLTYPE="mirror"
else                              POOLTYPE="basic"
fi

# Build JSON device array.
DEVICE_JSON="["
for i in "${!DEVICES[@]}"; do
    [ "$i" -gt 0 ] && DEVICE_JSON+=","
    DEVICE_JSON+="\"${DEVICES[$i]}\""
done
DEVICE_JSON+="]"

section "Configuration"
info "Devices  : ${DEVICES[*]}"
info "Pool type: $POOLTYPE"
info "Pool name: $POOL"

# OMV sentinel UUID used to signal "create new object" to $db->set().
OMV_NEW_UUID=$(. /etc/default/openmediavault 2>/dev/null; echo "${OMV_CONFIGOBJECT_NEW_UUID:-fa4b1c66-ef79-11e5-87a0-0002b3a176b4}")

# ===========================================================================
section "Informational RPCs (no pool required)"
# ===========================================================================

assert_rpc "getStats"             "Zfs" "getStats"
assert_rpc "listCompressionTypes" "Zfs" "listCompressionTypes"
assert_rpc "getEmptyCandidates"   "Zfs" "getEmptyCandidates"
assert_rpc "getArcStats"          "Zfs" "getArcStats" "{}" "ARC Size"

# ===========================================================================
section "Pool — create"
# ===========================================================================

ADDPOOL_PARAMS=$(python3 -c "
import json, sys
print(json.dumps({
    'pooltype':    '$POOLTYPE',
    'force':       True,
    'mountpoint':  '',
    'name':        '$POOL',
    'devices':     $DEVICE_JSON,
    'devalias':    'dev',
    'ashift':      False,
    'ashiftval':   0,
    'compress':    False,
    'compresstype':'lz4',
}))
")
assert_rpc "addPool ($POOLTYPE)" "Zfs" "addPool" "$ADDPOOL_PARAMS"
# addPool returns null on success; verify independently.
if zpool list "$POOL" &>/dev/null; then
    _pass "addPool — pool $POOL exists"
else
    _fail "addPool — pool $POOL not found after create" ""
fi

# ===========================================================================
section "Pool — list, details, properties"
# ===========================================================================

LIST_PARAMS='{"start":0,"limit":null,"sortfield":null,"sortdir":null}'
assert_rpc    "listPools"             "Zfs" "listPools"    "$LIST_PARAMS" "$POOL"
assert_rpc_bg "listPoolsBg"          "Zfs" "listPoolsBg"  "$LIST_PARAMS"

assert_rpc "getObjectDetails (Pool)" "Zfs" "getObjectDetails" \
    "{\"name\":\"$POOL\",\"type\":\"Pool\"}" "Pool status"

assert_rpc "getProperties (Pool)" "Zfs" "getProperties" \
    "{\"name\":\"$POOL\",\"type\":\"Pool\",\"start\":0,\"limit\":null}" \
    "compression"

assert_rpc "setProperties (Pool) — compression=lz4" "Zfs" "setProperties" \
    "{\"name\":\"$POOL\",\"type\":\"Pool\",\"properties\":[{\"property\":\"compression\",\"value\":\"lz4\",\"modified\":true}]}"

assert_rpc "scrubPool" "Zfs" "scrubPool" "{\"name\":\"$POOL\"}"

assert_rpc "getPoolHealth" "Zfs" "getPoolHealth" "{}" "$POOL"
assert_rpc "getPoolNames"  "Zfs" "getPoolNames"  "{}" "$POOL"

# ===========================================================================
section "Filesystem — add, details, properties"
# ===========================================================================

assert_rpc "addObject — filesystem fs1" "Zfs" "addObject" \
    "{\"type\":\"filesystem\",\"path\":\"$POOL\",\"name\":\"fs1\",\"mountpoint\":\"\"}"

assert_rpc "addObject — filesystem fs2 (custom mountpoint)" "Zfs" "addObject" \
    "{\"type\":\"filesystem\",\"path\":\"$POOL\",\"name\":\"fs2\",\"mountpoint\":\"/${POOL}_fs2\"}"

assert_rpc "addObject — nested filesystem fs1/child" "Zfs" "addObject" \
    "{\"type\":\"filesystem\",\"path\":\"$POOL/fs1\",\"name\":\"child\",\"mountpoint\":\"\"}"

assert_rpc "getObjectDetails (Filesystem)" "Zfs" "getObjectDetails" \
    "{\"name\":\"$POOL/fs1\",\"type\":\"Filesystem\"}" "mountpoint"

assert_rpc "getProperties (Filesystem)" "Zfs" "getProperties" \
    "{\"name\":\"$POOL/fs1\",\"type\":\"Filesystem\",\"start\":0,\"limit\":null}" \
    "compression"

assert_rpc "setProperties (Filesystem) — compression=lz4" "Zfs" "setProperties" \
    "{\"name\":\"$POOL/fs1\",\"type\":\"Filesystem\",\"properties\":[{\"property\":\"compression\",\"value\":\"lz4\",\"modified\":true}]}"

assert_rpc "setProperties (Filesystem) — atime=off" "Zfs" "setProperties" \
    "{\"name\":\"$POOL/fs1\",\"type\":\"Filesystem\",\"properties\":[{\"property\":\"atime\",\"value\":\"off\",\"modified\":true}]}"

assert_rpc "getDatasetNames" "Zfs" "getDatasetNames" "{}" "$POOL"

# ===========================================================================
section "Snapshot — add, list, rollback, delete"
# ===========================================================================

assert_rpc "addObject — snapshot fs1@snap1" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap1\"}"

SNAP_LIST_PARAMS='{"start":0,"limit":null,"sortfield":null,"sortdir":null}'
assert_rpc    "getAllSnapshots"   "Zfs" "getAllSnapshots"   "$SNAP_LIST_PARAMS" "snap1"
assert_rpc_bg "getAllSnapshotsBg" "Zfs" "getAllSnapshotsBg" "$SNAP_LIST_PARAMS"

assert_rpc "getObjectDetails (Snapshot)" "Zfs" "getObjectDetails" \
    "{\"name\":\"$POOL/fs1@snap1\",\"type\":\"Snapshot\"}" "creation"

# Write a file, roll back, verify it is gone.
TESTFILE="/$POOL/fs1/rollback_marker_$$"
touch "$TESTFILE" 2>/dev/null || true
assert_rpc "rollbackSnapshot — fs1@snap1" "Zfs" "rollbackSnapshot" \
    "{\"name\":\"$POOL/fs1@snap1\"}"
if [ ! -e "$TESTFILE" ]; then
    _pass "rollbackSnapshot — marker file removed as expected"
else
    _fail "rollbackSnapshot — marker file still present" "rollback may not have worked"
fi

assert_rpc "deleteObject — snapshot fs1@snap1" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1@snap1\",\"mp\":\"\",\"type\":\"Snapshot\"}"

# ===========================================================================
section "Clone — create and delete"
# ===========================================================================

assert_rpc "addObject — snapshot fs1@snap2 (for clone)" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap2\"}"

assert_rpc "addObject — clone of fs1@snap2" "Zfs" "addObject" \
    "{\"type\":\"clone\",\"path\":\"$POOL/fs1@snap2\",\"pool\":\"$POOL\",\"name\":\"\",\"clonename\":\"clone1\"}"

assert_rpc "deleteObject — clone1" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/clone1\",\"mp\":\"/$POOL/clone1\",\"type\":\"Filesystem\"}"

assert_rpc "deleteObject — snapshot fs1@snap2" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1@snap2\",\"mp\":\"\",\"type\":\"Snapshot\"}"

# ===========================================================================
section "Scheduled Snapshot Jobs — CRUD and run"
# ===========================================================================

SNAP_JOB_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'uuid':              '$OMV_NEW_UUID',
    'enable':            True,
    'dataset':           '$POOL/fs1',
    'prefix':            'test-snap-',
    'retention':         5,
    'execution':         'daily',
    'minute':            ['0'],
    'hour':              ['2'],
    'dayofmonth':        ['*'],
    'month':             ['*'],
    'dayofweek':         ['*'],
    'everynminute':      False,
    'everynhour':        False,
    'everyndayofmonth':  False,
    'sendemail':         False,
    'comment':           'test snapshot job',
}))
")
SNAP_JOB_RAW=$(rpc "Zfs" "setSnapshotJob" "$SNAP_JOB_PARAMS" 2>&1) && {
    SNAP_JOB_UUID=$(echo "$SNAP_JOB_RAW" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('uuid',''))" 2>/dev/null || echo "")
    if [ -n "$SNAP_JOB_UUID" ]; then
        _pass "setSnapshotJob — create"
    else
        _fail "setSnapshotJob — create" "UUID not found in response: ${SNAP_JOB_RAW:0:200}"
    fi
} || {
    _fail "setSnapshotJob — create" "${SNAP_JOB_RAW:0:200}"
    SNAP_JOB_UUID=""
}

JOB_LIST_PARAMS='{"start":0,"limit":null,"sortfield":null,"sortdir":null}'
assert_rpc "getSnapshotJobList" "Zfs" "getSnapshotJobList" "$JOB_LIST_PARAMS" "$POOL"

if [ -n "$SNAP_JOB_UUID" ]; then
    assert_rpc    "getSnapshotJob"      "Zfs" "getSnapshotJob"      "{\"uuid\":\"$SNAP_JOB_UUID\"}" "test-snap-"
    assert_rpc_bg "runSnapshotJobBg"    "Zfs" "runSnapshotJobBg"    "{\"uuid\":\"$SNAP_JOB_UUID\"}"
    assert_rpc    "deleteSnapshotJob"   "Zfs" "deleteSnapshotJob"   "{\"uuid\":\"$SNAP_JOB_UUID\"}"
else
    _fail "skipping getSnapshotJob/runSnapshotJobBg/deleteSnapshotJob — no UUID" ""
fi

# ===========================================================================
section "Scheduled Scrub Jobs — CRUD and run"
# ===========================================================================

SCRUB_JOB_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'uuid':              '$OMV_NEW_UUID',
    'enable':            True,
    'pool':              '$POOL',
    'execution':         'weekly',
    'minute':            ['0'],
    'hour':              ['0'],
    'dayofmonth':        ['*'],
    'month':             ['*'],
    'dayofweek':         ['0'],
    'everynminute':      False,
    'everynhour':        False,
    'everyndayofmonth':  False,
    'sendemail':         False,
    'comment':           'test scrub job',
}))
")
SCRUB_JOB_RAW=$(rpc "Zfs" "setScrubJob" "$SCRUB_JOB_PARAMS" 2>&1) && {
    SCRUB_JOB_UUID=$(echo "$SCRUB_JOB_RAW" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('uuid',''))" 2>/dev/null || echo "")
    if [ -n "$SCRUB_JOB_UUID" ]; then
        _pass "setScrubJob — create"
    else
        _fail "setScrubJob — create" "UUID not found in response: ${SCRUB_JOB_RAW:0:200}"
    fi
} || {
    _fail "setScrubJob — create" "${SCRUB_JOB_RAW:0:200}"
    SCRUB_JOB_UUID=""
}

assert_rpc "getScrubJobList" "Zfs" "getScrubJobList" "$JOB_LIST_PARAMS" "$POOL"

if [ -n "$SCRUB_JOB_UUID" ]; then
    assert_rpc    "getScrubJob"      "Zfs" "getScrubJob"      "{\"uuid\":\"$SCRUB_JOB_UUID\"}" "$POOL"
    assert_rpc_bg "runScrubJobBg"    "Zfs" "runScrubJobBg"    "{\"uuid\":\"$SCRUB_JOB_UUID\"}"
    assert_rpc    "deleteScrubJob"   "Zfs" "deleteScrubJob"   "{\"uuid\":\"$SCRUB_JOB_UUID\"}"
else
    _fail "skipping getScrubJob/runScrubJobBg/deleteScrubJob — no UUID" ""
fi

# ===========================================================================
section "Volume — thick and thin"
# ===========================================================================

assert_rpc "addObject — volume vol1 (thick, 100 MiB)" "Zfs" "addObject" \
    "{\"type\":\"volume\",\"path\":\"$POOL\",\"name\":\"vol1\",\"size\":\"104857600\",\"thinvol\":false}"

assert_rpc "addObject — volume vol2 (thin, 100 MiB)" "Zfs" "addObject" \
    "{\"type\":\"volume\",\"path\":\"$POOL\",\"name\":\"vol2\",\"size\":\"104857600\",\"thinvol\":true}"

assert_rpc "getObjectDetails (Volume)" "Zfs" "getObjectDetails" \
    "{\"name\":\"$POOL/vol1\",\"type\":\"Volume\"}" "volsize"

assert_rpc "getProperties (Volume)" "Zfs" "getProperties" \
    "{\"name\":\"$POOL/vol1\",\"type\":\"Volume\",\"start\":0,\"limit\":null}" \
    "volsize"

assert_rpc "deleteObject — vol1 (thick)" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/vol1\",\"mp\":\"\",\"type\":\"Volume\"}"

assert_rpc "deleteObject — vol2 (thin)" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/vol2\",\"mp\":\"\",\"type\":\"Volume\"}"

# ===========================================================================
section "Discover — add new (CLI-created dataset)"
# ===========================================================================

info "Creating $POOL/cli_fs directly via ZFS CLI (bypassing plugin)"
zfs create "$POOL/cli_fs"

assert_rpc_bg "doDiscoverBg — addMissing=true: picks up CLI-created dataset" \
    "Zfs" "doDiscoverBg" '{"addMissing":true,"deleteStale":false}'

if [ "$(mntent_exists "$POOL/cli_fs")" = "True" ]; then
    _pass "mntent entry created for $POOL/cli_fs"
else
    _fail "mntent entry for $POOL/cli_fs not found" "FsTab check returned False"
fi

# ===========================================================================
section "Discover — delete stale (CLI-destroyed dataset)"
# ===========================================================================

info "Destroying $POOL/cli_fs via ZFS CLI (leaving stale mntent entry)"
zfs destroy "$POOL/cli_fs"

assert_rpc_bg "doDiscoverBg — deleteStale=true: removes stale entry" \
    "Zfs" "doDiscoverBg" '{"addMissing":false,"deleteStale":true}'

if [ "$(mntent_exists "$POOL/cli_fs")" = "False" ]; then
    _pass "stale mntent entry for $POOL/cli_fs removed"
else
    _fail "stale mntent entry for $POOL/cli_fs still present" ""
fi

# ===========================================================================
section "Discover — addMissing=true does not error on stale entries"
# ===========================================================================

info "Creating $POOL/stale_fs, registering it, then destroying it via CLI"
zfs create "$POOL/stale_fs"
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":true,"deleteStale":false}' >/dev/null 2>&1 || true
zfs destroy "$POOL/stale_fs"

assert_rpc_bg "doDiscoverBg — addMissing=true with stale entry present: no error" \
    "Zfs" "doDiscoverBg" '{"addMissing":true,"deleteStale":false}'

# Clean up the stale entry.
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":false,"deleteStale":true}' >/dev/null 2>&1 || true

# ===========================================================================
section "Export and import"
# ===========================================================================

# Destroy child filesystems so the pool can be cleanly exported.
info "Removing child datasets before export test"
assert_rpc "deleteObject — fs1/child" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1/child\",\"mp\":\"/$POOL/fs1/child\",\"type\":\"Filesystem\"}"

# The scheduled snapshot job test may have created snapshots on fs1; remove them.
info "Removing any auto-created snapshots on fs1"
zfs list -H -t snapshot -o name -r "$POOL/fs1" 2>/dev/null | while IFS= read -r snap; do
    zfs destroy "$snap" 2>/dev/null || true
done

assert_rpc "deleteObject — fs1" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1\",\"mp\":\"/$POOL/fs1\",\"type\":\"Filesystem\"}"
assert_rpc "deleteObject — fs2" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs2\",\"mp\":\"/${POOL}_fs2\",\"type\":\"Filesystem\"}"

assert_rpc "exportPool" "Zfs" "exportPool" "{\"name\":\"$POOL\"}"

LIST_OUT=$(omv-rpc -u admin "Zfs" "listPools" "$LIST_PARAMS" 2>/dev/null || echo "")
if ! echo "$LIST_OUT" | grep -q "\"$POOL\""; then
    _pass "Pool absent from listPools after export"
else
    _fail "Pool still visible after export" ""
fi

assert_rpc "importPool — by name" "Zfs" "importPool" \
    "{\"poolname\":\"$POOL\",\"all\":false,\"force\":false}"

LIST_OUT=$(omv-rpc -u admin "Zfs" "listPools" "$LIST_PARAMS" 2>/dev/null || echo "")
if echo "$LIST_OUT" | grep -q "\"$POOL\""; then
    _pass "Pool visible in listPools after import"
else
    _fail "Pool not visible after import" ""
fi

# ===========================================================================
section "Pool — delete"
# ===========================================================================

assert_rpc_bg "deleteObjectBg — Pool" "Zfs" "deleteObjectBg" \
    "{\"name\":\"$POOL\",\"mp\":\"/$POOL\",\"type\":\"Pool\"}"

if ! zpool list "$POOL" &>/dev/null; then
    _pass "Pool $POOL destroyed"
else
    _fail "Pool $POOL still exists after deleteObjectBg" ""
fi

# ===========================================================================
section "Summary"
# ===========================================================================

echo
echo -e "${BOLD}Results: ${GREEN}${PASS} passed${NC}  ${RED}${FAIL} failed${NC}"
if [ "${#FAILED_TESTS[@]}" -gt 0 ]; then
    echo -e "\n${RED}Failed tests:${NC}"
    for t in "${FAILED_TESTS[@]}"; do
        echo -e "  • $t"
    done
fi
echo

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
