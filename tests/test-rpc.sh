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
STRIPE_POOL=""   # set later if a temporary vdev-removal test pool is created

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
# When the bg task throws an exception, OMV stores the error in the status
# file and Exec.isRunning re-raises it as a TraceException (omv-rpc exits
# non-zero with a JSON error body).  We detect this to avoid false PASSes.
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
    local timeout=120 elapsed=0 poll_ec poll_out
    while [ $elapsed -lt $timeout ]; do
        poll_out=$(omv-rpc -u admin "Exec" "isRunning" \
            "{\"filename\":\"$filename\"}" 2>&1)
        poll_ec=$?
        # Non-zero exit: either the bg task threw (TraceException) or the
        # status file was already cleaned up — either way the task is done.
        [ $poll_ec -ne 0 ] && break
        echo "$poll_out" | grep -q '"running":true\|"running": true' || break
        sleep 2; ((elapsed += 2)) || true
    done
    if [ $elapsed -ge $timeout ]; then
        _fail "$desc" "Bg task timed out after ${timeout}s"
        return 1
    fi

    # If isRunning threw a TraceException the bg task failed — extract the
    # error message from the JSON error response.
    if [ $poll_ec -ne 0 ]; then
        local err
        err=$(echo "$poll_out" | python3 -c \
            "import sys,json
d=json.load(sys.stdin)
e=d.get('error') or {}
print(e.get('message', str(d))[:300])" 2>/dev/null \
            || echo "${poll_out:0:200}")
        _fail "$desc" "$err"
        return 1
    fi

    # Task completed without error.  getOutput may also fail if the status
    # file was already cleaned up; that is fine — it means there is no
    # extra output to inspect.
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

# Return the dir field of the OMV FsTab entry for the given fsname, or empty string.
mntent_dir() {
    omv-rpc -u admin "FsTab" "enumerateEntries" '{}' 2>/dev/null \
        | python3 -c "
import sys, json
entries = json.load(sys.stdin)
match = next((e for e in entries if e.get('fsname') == '$1'), None)
print(match['dir'] if match else '')
" 2>/dev/null || echo ""
}

# ---------------------------------------------------------------------------
# Cleanup — always runs on exit
# ---------------------------------------------------------------------------
cleanup() {
    section "Cleanup"
    info "Destroying pool $POOL (if it exists)"
    zpool destroy -f "$POOL" 2>/dev/null || true
    info "Destroying temporary stripe pool ${STRIPE_POOL:-} (if it exists)"
    [ -n "${STRIPE_POOL:-}" ] && zpool destroy -f "$STRIPE_POOL" 2>/dev/null || true
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
# Determine pool type from device count.
# With exactly 4 devices, reserve the last one for the RAIDZ expansion test
# and create a 3-device raidz1 as the main pool.
# ---------------------------------------------------------------------------
DEVCOUNT=${#DEVICES[@]}
EXPAND_DEV=""      # device reserved for RAIDZ expansion test
LOG_DEV_1=""       # first log device     (log mirror test)
LOG_DEV_2=""       # second log device    (log mirror test)
SPECIAL_DEV_1=""   # first special device  (special mirror test)
SPECIAL_DEV_2=""   # second special device (special mirror test)

# Device allocation strategy:
#   1 dev  → basic pool
#   2 devs → mirror pool
#   3 devs → raidz1 pool
#   4 devs → raidz1 (3) + RAIDZ expansion device (1)
#   5 devs → raidz1 (3) + log mirror (2)                         [no expansion]
#   6 devs → raidz1 (3) + RAIDZ expansion (1) + log mirror (2)
#   7 devs → raidz1 (3) + log mirror (2) + special mirror (2)    [no expansion]
#   8 devs → raidz1 (3) + RAIDZ expansion (1) + log mirror (2) + special mirror (2)
#   9+     → raidz2 pool (all devices, no extra tests)
if [ "$DEVCOUNT" -eq 4 ]; then
    POOL_DEVICES=("${DEVICES[@]:0:3}")
    EXPAND_DEV="${DEVICES[3]}"
    POOLTYPE="raidz1"
elif [ "$DEVCOUNT" -eq 5 ]; then
    POOL_DEVICES=("${DEVICES[@]:0:3}")
    LOG_DEV_1="${DEVICES[3]}"
    LOG_DEV_2="${DEVICES[4]}"
    POOLTYPE="raidz1"
elif [ "$DEVCOUNT" -eq 6 ]; then
    POOL_DEVICES=("${DEVICES[@]:0:3}")
    EXPAND_DEV="${DEVICES[3]}"
    LOG_DEV_1="${DEVICES[4]}"
    LOG_DEV_2="${DEVICES[5]}"
    POOLTYPE="raidz1"
elif [ "$DEVCOUNT" -eq 7 ]; then
    POOL_DEVICES=("${DEVICES[@]:0:3}")
    LOG_DEV_1="${DEVICES[3]}"
    LOG_DEV_2="${DEVICES[4]}"
    SPECIAL_DEV_1="${DEVICES[5]}"
    SPECIAL_DEV_2="${DEVICES[6]}"
    POOLTYPE="raidz1"
elif [ "$DEVCOUNT" -eq 8 ]; then
    POOL_DEVICES=("${DEVICES[@]:0:3}")
    EXPAND_DEV="${DEVICES[3]}"
    LOG_DEV_1="${DEVICES[4]}"
    LOG_DEV_2="${DEVICES[5]}"
    SPECIAL_DEV_1="${DEVICES[6]}"
    SPECIAL_DEV_2="${DEVICES[7]}"
    POOLTYPE="raidz1"
else
    POOL_DEVICES=("${DEVICES[@]}")
    if   [ "$DEVCOUNT" -ge 9 ]; then POOLTYPE="raidz2"
    elif [ "$DEVCOUNT" -ge 3 ]; then POOLTYPE="raidz1"
    elif [ "$DEVCOUNT" -ge 2 ]; then POOLTYPE="mirror"
    else                              POOLTYPE="basic"
    fi
fi

# Build JSON device array from the pool devices (may be a subset of DEVICES).
DEVICE_JSON="["
for i in "${!POOL_DEVICES[@]}"; do
    [ "$i" -gt 0 ] && DEVICE_JSON+=","
    DEVICE_JSON+="\"${POOL_DEVICES[$i]}\""
done
DEVICE_JSON+="]"

section "Configuration"
info "Devices  : ${DEVICES[*]}"
info "Pool type: $POOLTYPE"
info "Pool name: $POOL"
[ -n "$EXPAND_DEV"   ] && info "Expansion  : $EXPAND_DEV (reserved for RAIDZ expansion test)"
[ -n "$LOG_DEV_1"    ] && info "Log devs   : $LOG_DEV_1  $LOG_DEV_2 (reserved for log mirror test)"
[ -n "$SPECIAL_DEV_1" ] && info "Special devs: $SPECIAL_DEV_1  $SPECIAL_DEV_2 (reserved for special mirror test)"

# OMV sentinel UUID used to signal "create new object" to $db->set().
OMV_NEW_UUID=$(. /etc/default/openmediavault 2>/dev/null; echo "${OMV_CONFIGOBJECT_NEW_UUID:-fa4b1c66-ef79-11e5-87a0-0002b3a176b4}")

# ===========================================================================
section "Informational RPCs (no pool required)"
# ===========================================================================

assert_rpc "getStats"             "Zfs" "getStats"
assert_rpc "listCompressionTypes" "Zfs" "listCompressionTypes"
assert_rpc "getEmptyCandidates"   "Zfs" "getEmptyCandidates"
assert_rpc "getArcStats"          "Zfs" "getArcStats"  # returns plain text, no JSON pattern

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

# Enable raidz_expansion if we reserved a device for the expansion test.
if [ -n "$EXPAND_DEV" ]; then
    if zpool set feature@raidz_expansion=enabled "$POOL" 2>/dev/null; then
        _pass "addPool — feature@raidz_expansion enabled for expansion test"
    else
        _fail "addPool — could not enable feature@raidz_expansion" \
              "RAIDZ expansion test will be skipped"
    fi
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
    "autoexpand"

assert_rpc "setProperties (Pool) — comment" "Zfs" "setProperties" \
    "{\"name\":\"$POOL\",\"type\":\"Pool\",\"properties\":[{\"property\":\"comment\",\"value\":\"omvzfstest\",\"modified\":true}]}"

assert_rpc "scrubPool" "Zfs" "scrubPool" "{\"name\":\"$POOL\"}"

assert_rpc "getPoolHealth" "Zfs" "getPoolHealth" "{}" "$POOL"
assert_rpc "getPoolNames"  "Zfs" "getPoolNames"  "{}" "$POOL"

# ===========================================================================
section "Device management — pool devices, top-level vdevs, vdev removal"
# ===========================================================================

assert_rpc "getPoolDevices"       "Zfs" "getPoolDevices"      "{\"name\":\"$POOL\"}"
assert_rpc "getTopLevelVdevs"     "Zfs" "getTopLevelVdevs"    "{\"name\":\"$POOL\"}"

# Verify getTopLevelVdevs result matches expected removability for the pool type.
# Raidz vdevs cannot be removed, so an empty list is correct for raidz pools.
# Mirror and stripe pools must have at least one removable entry.
VDEV_JSON=$(rpc "Zfs" "getTopLevelVdevs" "{\"name\":\"$POOL\"}" 2>/dev/null || echo "[]")
VDEV_COUNT=$(echo "$VDEV_JSON" | python3 -c "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo 0)
case "$POOLTYPE" in
    raidz*)
        if [ "$VDEV_COUNT" -eq 0 ]; then
            _pass "getTopLevelVdevs — raidz vdevs correctly excluded (not removable)"
        else
            _fail "getTopLevelVdevs — raidz vdevs should not appear in removable list" \
                  "got $VDEV_COUNT entries"
        fi
        ;;
    *)
        if [ "$VDEV_COUNT" -gt 0 ]; then
            _pass "getTopLevelVdevs — returned $VDEV_COUNT removable vdev(s) for $POOLTYPE pool"
        else
            _fail "getTopLevelVdevs — expected removable vdevs for $POOLTYPE pool, got none" ""
        fi
        ;;
esac

# getVdevRemovalStatus returns a string (no removal in progress on a fresh pool).
assert_rpc "getVdevRemovalStatus" "Zfs" "getVdevRemovalStatus" "{\"name\":\"$POOL\"}" \
    "No vdev removal in progress"

# removeVdev — verify the RPC is callable and returns a proper error for an
# invalid vdev rather than crashing.  A full functional evacuation test requires
# a multi-vdev stripe pool with a free device, which is not possible here because
# all supplied devices are consumed by the main pool.
assert_rpc_fails "removeVdev — invalid vdev name returns error" \
    "Zfs" "removeVdev" "{\"pool\":\"$POOL\",\"vdev\":\"nonexistent-vdev-$$\"}"

# cancelVdevRemoval — no removal in progress, should return an error.
assert_rpc_fails "cancelVdevRemoval — no removal active returns error" \
    "Zfs" "cancelVdevRemoval" "{\"name\":\"$POOL\"}"

# ===========================================================================
section "getAttachAnchors — verify anchor list for pool type"
# ===========================================================================

ANCHORS_OUT=$(rpc "Zfs" "getAttachAnchors" "{\"name\":\"$POOL\"}" 2>/dev/null || echo "[]")
ANCHOR_COUNT=$(echo "$ANCHORS_OUT" | python3 -c \
    "import sys,json; print(len(json.load(sys.stdin)))" 2>/dev/null || echo 0)

case "$POOLTYPE" in
    raidz*)
        # For RAIDZ pools: anchors are the vdev name when raidz_expansion is enabled/active,
        # or empty when expansion is not enabled (member devices are never valid anchors).
        RAIDZ_ANCHOR=$(echo "$ANCHORS_OUT" | python3 -c "
import sys, json
items = json.load(sys.stdin)
vdev = next((i['devicefile'] for i in items if 'RAIDZ expansion' in i.get('label','')), '')
print(vdev)
" 2>/dev/null || echo "")
        if [ -n "$RAIDZ_ANCHOR" ]; then
            _pass "getAttachAnchors — RAIDZ expansion anchor present: $RAIDZ_ANCHOR"
        elif [ -n "$EXPAND_DEV" ]; then
            # Feature was explicitly enabled above — anchor must be present.
            _fail "getAttachAnchors — feature@raidz_expansion is enabled but no anchor returned" \
                  "raw: ${ANCHORS_OUT:0:200}"
        else
            _pass "getAttachAnchors — no RAIDZ expansion anchor (feature not enabled; empty list is correct)"
        fi
        # Verify that no RAIDZ member device paths leaked into the anchor list.
        HAS_MEMBER=$(echo "$ANCHORS_OUT" | python3 -c "
import sys, json
items = json.load(sys.stdin)
print(any(i['devicefile'].startswith('/') for i in items))
" 2>/dev/null || echo "False")
        if [ "$HAS_MEMBER" = "False" ]; then
            _pass "getAttachAnchors — RAIDZ member devices correctly excluded from anchor list"
        else
            _fail "getAttachAnchors — RAIDZ member device paths must not appear as anchors" \
                  "got: ${ANCHORS_OUT:0:200}"
        fi
        ;;
    *)
        # Mirror/stripe pools: anchors should be device paths, not vdev names.
        if [ "$ANCHOR_COUNT" -gt 0 ]; then
            _pass "getAttachAnchors — returned $ANCHOR_COUNT anchor(s)"
        else
            _fail "getAttachAnchors — expected at least one anchor for $POOLTYPE pool" ""
        fi
        HAS_DEV=$(echo "$ANCHORS_OUT" | python3 -c "
import sys, json
items = json.load(sys.stdin)
print(any(i['devicefile'].startswith('/') for i in items))
" 2>/dev/null || echo "False")
        if [ "$HAS_DEV" = "True" ]; then
            _pass "getAttachAnchors — device paths returned for $POOLTYPE pool"
        else
            _fail "getAttachAnchors — expected device paths for $POOLTYPE pool" \
                  "got: ${ANCHORS_OUT:0:200}"
        fi
        ;;
esac

# ===========================================================================
section "deviceDetach on RAIDZ — expect clean error, not exception"
# ===========================================================================

case "$POOLTYPE" in
    raidz*)
        # Pick the first device from the pool status directly (RAIDZ members no longer
        # appear in getAttachAnchors, so we query zpool status instead).
        RAIDZ_DEV=$(zpool status -P "$POOL" 2>/dev/null \
            | awk '/^\s+\//{print $1; exit}')
        if [ -n "$RAIDZ_DEV" ]; then
            assert_rpc_fails "deviceDetach on RAIDZ — clean error (not stack trace)" \
                "Zfs" "deviceDetach" \
                "{\"pool\":\"$POOL\",\"device\":\"$RAIDZ_DEV\"}"
            # Verify the error message is the zpool message, not a PHP trace.
            DETACH_OUT=$(omv-rpc -u admin "Zfs" "deviceDetach" \
                "{\"pool\":\"$POOL\",\"device\":\"$RAIDZ_DEV\"}" 2>&1 || true)
            if echo "$DETACH_OUT" | grep -qi "only applicable\|not supported\|cannot detach"; then
                _pass "deviceDetach on RAIDZ — error message is from zpool (not PHP trace)"
            elif echo "$DETACH_OUT" | grep -qi "Stack trace\|#[0-9]"; then
                _fail "deviceDetach on RAIDZ — PHP stack trace leaked to output" \
                      "${DETACH_OUT:0:200}"
            else
                _pass "deviceDetach on RAIDZ — failed with non-trace message (acceptable)"
            fi
        else
            _fail "deviceDetach on RAIDZ — could not determine a device to test with" ""
        fi
        ;;
    *)
        info "Skipping RAIDZ detach error test (pool type is $POOLTYPE)"
        ;;
esac

# ===========================================================================
section "RAIDZ expansion via deviceAttach (requires 4 devices)"
# ===========================================================================

if [ -n "$EXPAND_DEV" ] && [ "$POOLTYPE" = "raidz1" ]; then
    # Determine the RAIDZ expansion anchor from getAttachAnchors.
    RAIDZ_ANCHOR=$(echo "$ANCHORS_OUT" | python3 -c "
import sys, json
items = json.load(sys.stdin)
vdev = next((i['devicefile'] for i in items if 'RAIDZ expansion' in i.get('label','')), '')
print(vdev)
" 2>/dev/null || echo "")

    if [ -n "$RAIDZ_ANCHOR" ]; then
        info "Expanding $RAIDZ_ANCHOR with $EXPAND_DEV"
        ATTACH_PARAMS=$(python3 -c "
import json
print(json.dumps({'pool': '$POOL', 'olddevice': '$RAIDZ_ANCHOR', 'newdevice': '$EXPAND_DEV'}))
")
        assert_rpc "deviceAttach — RAIDZ expansion ($RAIDZ_ANCHOR + $EXPAND_DEV)" \
            "Zfs" "deviceAttach" "$ATTACH_PARAMS"

        # Give ZFS a moment to register the new device then check pool config.
        sleep 2
        POOL_STATUS=$(zpool status "$POOL" 2>/dev/null || echo "")
        EXPAND_BASENAME=$(basename "$EXPAND_DEV")
        if echo "$POOL_STATUS" | grep -q "$EXPAND_DEV\|$EXPAND_BASENAME"; then
            _pass "deviceAttach — expansion device visible in pool config"
        else
            _fail "deviceAttach — expansion device not found in pool config" \
                  "$(echo "$POOL_STATUS" | grep -E 'NAME|raidz|scsi|sd[a-z]' | head -8)"
        fi

        # The pool status should show an expand: line or the device resilvering.
        if echo "$POOL_STATUS" | grep -qE "expand:|resilvering|resilvered"; then
            _pass "deviceAttach — pool shows expansion or resilver activity"
        else
            _pass "deviceAttach — expansion completed immediately (small pool)"
        fi
    else
        _fail "deviceAttach — RAIDZ expansion skipped: feature@raidz_expansion not active on $POOL" \
              "Enable with: zpool set feature@raidz_expansion=enabled $POOL"
    fi
else
    if [ -z "$EXPAND_DEV" ]; then
        info "Skipping RAIDZ expansion test (pass exactly 4 devices to enable)"
    else
        info "Skipping RAIDZ expansion test (pool type is $POOLTYPE, need raidz1)"
    fi
fi

# ===========================================================================
section "Log vdev — addVdev creates a mirror (not a stripe)"
# ===========================================================================
# Regression: the second 'zpool add ... log dev' call used to fall through
# to another 'zpool add' (stripe) because findFirstLogDevice matched 'logs'
# at topDepth instead of poolDepth.  The fix makes the second add use
# 'zpool attach' so the result is a mirrored log vdev.

if [ -n "$LOG_DEV_1" ] && [ -n "$LOG_DEV_2" ]; then
    ADDVDEV1_PARAMS=$(python3 -c "
import json
print(json.dumps({'pool': '$POOL', 'vdevtype': 'log', 'device': '$LOG_DEV_1', 'devalias': 'dev'}))
")
    ADDVDEV2_PARAMS=$(python3 -c "
import json
print(json.dumps({'pool': '$POOL', 'vdevtype': 'log', 'device': '$LOG_DEV_2', 'devalias': 'dev'}))
")

    assert_rpc "addVdev — add first log device" "Zfs" "addVdev" "$ADDVDEV1_PARAMS"
    assert_rpc "addVdev — add second log device (should attach, not add)" "Zfs" "addVdev" "$ADDVDEV2_PARAMS"

    # Allow ZFS a moment to settle then inspect the pool config.
    sleep 1
    LOG_STATUS=$(zpool status -P "$POOL" 2>/dev/null || echo "")

    # Both devices must appear in the pool.
    LOG1_BASE=$(basename "$LOG_DEV_1")
    LOG2_BASE=$(basename "$LOG_DEV_2")
    if echo "$LOG_STATUS" | grep -qE "$LOG_DEV_1|$LOG1_BASE"; then
        _pass "addVdev — first log device visible in pool"
    else
        _fail "addVdev — first log device not found in pool status" \
              "$(echo "$LOG_STATUS" | grep -A10 'logs' | head -8)"
    fi
    if echo "$LOG_STATUS" | grep -qE "$LOG_DEV_2|$LOG2_BASE"; then
        _pass "addVdev — second log device visible in pool"
    else
        _fail "addVdev — second log device not found in pool status" \
              "$(echo "$LOG_STATUS" | grep -A10 'logs' | head -8)"
    fi

    # The critical check: a 'mirror-' vdev must appear under the logs section.
    # A stripe would show the devices directly under 'logs' with no mirror label.
    LOGS_SECTION=$(echo "$LOG_STATUS" | awk '/^[[:space:]]*logs[[:space:]]*$/{found=1; next} found && /^[[:space:]]*[^[:space:]]/{if ($1 != "errors:") print; else exit} found && /^[[:space:]]+/{print}' )
    if echo "$LOGS_SECTION" | grep -q "mirror-"; then
        _pass "addVdev — log devices form a mirror (mirror-X vdev present under logs)"
    else
        _fail "addVdev — log devices are a STRIPE, not a mirror (mirror-X missing under logs)" \
              "logs section: $(echo "$LOG_STATUS" | grep -A5 'logs' | head -6)"
    fi

    # Regression: getTopLevelVdevs must label the log mirror as type 'log', not 'data'.
    # Bug: class headers (logs, cache) appear at poolDepth, not topDepth, so they were
    # missed and the mirror vdev under 'logs' was incorrectly labelled as a data vdev.
    TOPLEVEL_JSON=$(rpc "Zfs" "getTopLevelVdevs" "{\"name\":\"$POOL\"}" 2>/dev/null || echo "[]")
    LOG_TYPE_COUNT=$(echo "$TOPLEVEL_JSON" | python3 -c \
        "import sys,json; d=json.load(sys.stdin); print(sum(1 for v in d if v.get('type')=='log'))" \
        2>/dev/null || echo 0)
    DATA_MISLABELED=$(echo "$TOPLEVEL_JSON" | python3 -c "
import sys, json
d = json.load(sys.stdin)
bad = [v for v in d if v.get('type') in ('mirror','stripe','unknown') and 'Log:' in v.get('label','')]
print(len(bad))
" 2>/dev/null || echo 0)
    if [ "$LOG_TYPE_COUNT" -gt 0 ]; then
        _pass "getTopLevelVdevs — log mirror correctly labelled as type 'log' ($LOG_TYPE_COUNT entry)"
    else
        _fail "getTopLevelVdevs — log mirror not found or mis-labelled as data" \
              "result: $TOPLEVEL_JSON"
    fi
    if [ "$DATA_MISLABELED" -eq 0 ]; then
        _pass "getTopLevelVdevs — no log/cache vdevs mis-labelled as data type"
    else
        _fail "getTopLevelVdevs — $DATA_MISLABELED log/cache vdev(s) mis-labelled as data" \
              "result: $TOPLEVEL_JSON"
    fi
else
    info "Skipping log mirror test (pass 5 or 6 or 7 or 8 devices to enable)"
fi

# ===========================================================================
section "Special vdev — addVdev creates a mirror (not a stripe)"
# ===========================================================================
# A non-redundant special vdev risks pool data loss on device failure, so the
# second addVdev call must use 'zpool attach' (mirror) rather than 'zpool add'
# (stripe).  Mirrors are recognised by a 'mirror-' token under the 'special'
# section in zpool status.

if [ -n "$SPECIAL_DEV_1" ] && [ -n "$SPECIAL_DEV_2" ]; then
    ADDSPECIAL1_PARAMS=$(python3 -c "
import json
print(json.dumps({'pool': '$POOL', 'vdevtype': 'special', 'device': '$SPECIAL_DEV_1', 'devalias': 'dev'}))
")
    ADDSPECIAL2_PARAMS=$(python3 -c "
import json
print(json.dumps({'pool': '$POOL', 'vdevtype': 'special', 'device': '$SPECIAL_DEV_2', 'devalias': 'dev'}))
")

    assert_rpc "addVdev — add first special device" "Zfs" "addVdev" "$ADDSPECIAL1_PARAMS"
    assert_rpc "addVdev — add second special device (should attach, not add)" "Zfs" "addVdev" "$ADDSPECIAL2_PARAMS"

    sleep 1
    SPECIAL_STATUS=$(zpool status -P "$POOL" 2>/dev/null || echo "")

    SPEC1_BASE=$(basename "$SPECIAL_DEV_1")
    SPEC2_BASE=$(basename "$SPECIAL_DEV_2")
    if echo "$SPECIAL_STATUS" | grep -qE "$SPECIAL_DEV_1|$SPEC1_BASE"; then
        _pass "addVdev — first special device visible in pool"
    else
        _fail "addVdev — first special device not found in pool status" \
              "$(echo "$SPECIAL_STATUS" | grep -A10 'special' | head -8)"
    fi
    if echo "$SPECIAL_STATUS" | grep -qE "$SPECIAL_DEV_2|$SPEC2_BASE"; then
        _pass "addVdev — second special device visible in pool"
    else
        _fail "addVdev — second special device not found in pool status" \
              "$(echo "$SPECIAL_STATUS" | grep -A10 'special' | head -8)"
    fi

    # Critical check: a mirror-N vdev must appear under the special section.
    SPECIAL_SECTION=$(echo "$SPECIAL_STATUS" | awk \
        '/^[[:space:]]*special[[:space:]]*$/{found=1; next}
         found && /^[[:space:]]*[^[:space:]]/{if ($1 != "errors:") print; else exit}
         found && /^[[:space:]]+/{print}')
    if echo "$SPECIAL_SECTION" | grep -q "mirror-"; then
        _pass "addVdev — special devices form a mirror (mirror-N vdev present under special)"
    else
        _fail "addVdev — special devices are a STRIPE, not a mirror (mirror-N missing under special)" \
              "special section: $(echo "$SPECIAL_STATUS" | grep -A5 'special' | head -6)"
    fi

    # getTopLevelVdevs must label the special mirror as type 'special', not 'data'.
    TOPLEVEL_JSON=$(rpc "Zfs" "getTopLevelVdevs" "{\"name\":\"$POOL\"}" 2>/dev/null || echo "[]")
    SPECIAL_TYPE_COUNT=$(echo "$TOPLEVEL_JSON" | python3 -c \
        "import sys,json; d=json.load(sys.stdin); print(sum(1 for v in d if v.get('type')=='special'))" \
        2>/dev/null || echo 0)
    if [ "$SPECIAL_TYPE_COUNT" -gt 0 ]; then
        _pass "getTopLevelVdevs — special mirror correctly labelled as type 'special'"
    else
        _fail "getTopLevelVdevs — special mirror not found or mis-labelled" \
              "result: $TOPLEVEL_JSON"
    fi
else
    info "Skipping special mirror test (pass 7 or 8 devices to enable)"
fi

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
section "Snapshot — bulk delete (deleteSnapshotRange)"
# ===========================================================================

# Create snapshots snap-a, snap-b, snap-c for range deletion tests.
assert_rpc "addObject — snapshot fs1@snap-a" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap-a\"}"
assert_rpc "addObject — snapshot fs1@snap-b" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap-b\"}"
assert_rpc "addObject — snapshot fs1@snap-c" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap-c\"}"

# "earlier" mode: delete snap-b and all earlier (snap-a, snap-b); snap-c survives.
assert_rpc "deleteSnapshotRange — earlier (snap-b and all earlier)" "Zfs" "deleteSnapshotRange" \
    "{\"name\":\"$POOL/fs1@snap-b\",\"mode\":\"earlier\"}"
if ! zfs list -H -t snapshot -o name "$POOL/fs1@snap-c" >/dev/null 2>&1; then
    _fail "deleteSnapshotRange earlier — snap-c should still exist" ""
else
    _pass "deleteSnapshotRange earlier — snap-c survived as expected"
fi
if zfs list -H -t snapshot -o name "$POOL/fs1@snap-a" >/dev/null 2>&1; then
    _fail "deleteSnapshotRange earlier — snap-a should have been deleted" ""
else
    _pass "deleteSnapshotRange earlier — snap-a was deleted as expected"
fi

# Create new snapshots for "later" and "all" tests.
assert_rpc "addObject — snapshot fs1@snap-d" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap-d\"}"
assert_rpc "addObject — snapshot fs1@snap-e" "Zfs" "addObject" \
    "{\"type\":\"snapshot\",\"path\":\"$POOL/fs1\",\"name\":\"snap-e\"}"

# "later" mode: delete snap-d and all later (snap-d, snap-e); snap-c survives.
assert_rpc "deleteSnapshotRange — later (snap-d and all later)" "Zfs" "deleteSnapshotRange" \
    "{\"name\":\"$POOL/fs1@snap-d\",\"mode\":\"later\"}"
if ! zfs list -H -t snapshot -o name "$POOL/fs1@snap-c" >/dev/null 2>&1; then
    _fail "deleteSnapshotRange later — snap-c should still exist" ""
else
    _pass "deleteSnapshotRange later — snap-c survived as expected"
fi
if zfs list -H -t snapshot -o name "$POOL/fs1@snap-e" >/dev/null 2>&1; then
    _fail "deleteSnapshotRange later — snap-e should have been deleted" ""
else
    _pass "deleteSnapshotRange later — snap-e was deleted as expected"
fi

# "all" mode: delete all remaining snapshots on fs1.
assert_rpc "deleteSnapshotRange — all snapshots on fs1" "Zfs" "deleteSnapshotRange" \
    "{\"name\":\"$POOL/fs1@snap-c\",\"mode\":\"all\"}"
REMAINING=$(zfs list -H -t snapshot -o name 2>/dev/null | grep "^$POOL/fs1@" || true)
if [ -n "$REMAINING" ]; then
    _fail "deleteSnapshotRange all — snapshots still exist: $REMAINING" ""
else
    _pass "deleteSnapshotRange all — all snapshots removed as expected"
fi

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
section "Rename dataset — success and busy-error diagnostics"
# ===========================================================================

assert_rpc "addObject — filesystem for rename tests" "Zfs" "addObject" \
    "{\"type\":\"filesystem\",\"path\":\"$POOL\",\"name\":\"fs_rename_test\",\"mountpoint\":\"\"}"

# --- Success: rename and verify the ZFS name changed ---
assert_rpc "renameObject — rename fs_rename_test to fs_rename_ok" "Zfs" "renameObject" \
    "{\"oldpath\":\"$POOL/fs_rename_test\",\"oldname\":\"fs_rename_test\",\"type\":\"Filesystem\",\"newname\":\"fs_rename_ok\"}"

if zfs list -H -o name "$POOL/fs_rename_ok" >/dev/null 2>&1; then
    _pass "renameObject — new name fs_rename_ok is visible in ZFS"
else
    _fail "renameObject — new name not visible in ZFS after rename" ""
fi
if ! zfs list -H -o name "$POOL/fs_rename_test" >/dev/null 2>&1; then
    _pass "renameObject — old name fs_rename_test is gone"
else
    _fail "renameObject — old name still present in ZFS after rename" ""
fi

# --- Busy: hold the mountpoint open and verify the error identifies the cause ---
# A background sleep process with its CWD inside the mountpoint keeps ZFS from
# unmounting the dataset, causing 'zfs rename' to fail with "dataset is busy".
# The improved renameObject should surface the process name in the error message.
RENAME_OK_MP=$(zfs get -H -o value mountpoint "$POOL/fs_rename_ok" 2>/dev/null || echo "")
if [ -n "$RENAME_OK_MP" ] && [ "$RENAME_OK_MP" != "-" ] && [ "$RENAME_OK_MP" != "none" ]; then
    (cd "$RENAME_OK_MP" && exec sleep 30) &
    BUSY_PID=$!
    sleep 1   # Give the process time to establish its CWD in the mountpoint.

    BUSY_OUT=$(omv-rpc -u admin "Zfs" "renameObject" \
        "{\"oldpath\":\"$POOL/fs_rename_ok\",\"oldname\":\"fs_rename_ok\",\"type\":\"Filesystem\",\"newname\":\"fs_rename_busy_target\"}" \
        2>&1) || true

    kill "$BUSY_PID" 2>/dev/null || true
    wait "$BUSY_PID" 2>/dev/null || true

    if echo "$BUSY_OUT" | grep -qi "busy"; then
        _pass "renameObject busy — error message mentions 'busy'"
    else
        _fail "renameObject busy — 'busy' not found in error message" "${BUSY_OUT:0:300}"
    fi
    if echo "$BUSY_OUT" | grep -qi "sleep"; then
        _pass "renameObject busy — error identifies the holding process (sleep)"
    else
        _fail "renameObject busy — holding process not named in error message" "${BUSY_OUT:0:300}"
    fi
else
    info "Skipping busy-rename test (mountpoint not available: '$RENAME_OK_MP')"
fi

# Clean up — delete whichever name the dataset ended up with after the tests.
for _rn_ds in "$POOL/fs_rename_ok" "$POOL/fs_rename_busy_target" "$POOL/fs_rename_test"; do
    if zfs list -H -o name "$_rn_ds" >/dev/null 2>&1; then
        _rn_mp=$(zfs get -H -o value mountpoint "$_rn_ds" 2>/dev/null || echo "")
        assert_rpc "deleteObject — rename test cleanup" "Zfs" "deleteObject" \
            "{\"name\":\"$_rn_ds\",\"mp\":\"$_rn_mp\",\"type\":\"Filesystem\"}"
        break
    fi
done
unset _rn_ds _rn_mp

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
section "Cron files — omv-salt deploy generates valid entries"
# ===========================================================================
# Regression: Jinja2 whitespace bug split the >/dev/null redirect onto its
# own line, causing cron to log "Error: bad minute" on every reload.
# This section creates one snapshot job and one scrub job, deploys the cron
# state, then validates the generated files.

CRON_SNAP_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'uuid':             '$OMV_NEW_UUID',
    'enable':           True,
    'dataset':          '$POOL',
    'prefix':           'crontest-',
    'retention':        3,
    'execution':        'daily',
    'minute':           ['0'],
    'hour':             ['3'],
    'dayofmonth':       ['*'],
    'month':            ['*'],
    'dayofweek':        ['*'],
    'everynminute':     False,
    'everynhour':       False,
    'everyndayofmonth': False,
    'sendemail':        False,
    'comment':          'cron regression test',
}))
")
CRON_SCRUB_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'uuid':             '$OMV_NEW_UUID',
    'enable':           True,
    'pool':             '$POOL',
    'execution':        'weekly',
    'minute':           ['0'],
    'hour':             ['0'],
    'dayofmonth':       ['*'],
    'month':            ['*'],
    'dayofweek':        ['0'],
    'everynminute':     False,
    'everynhour':       False,
    'everyndayofmonth': False,
    'sendemail':        False,
    'comment':          'cron scrub regression test',
}))
")

CRON_SNAP_UUID=""
CRON_SCRUB_UUID=""

CRON_SNAP_RAW=$(rpc "Zfs" "setSnapshotJob" "$CRON_SNAP_PARAMS" 2>&1) && {
    CRON_SNAP_UUID=$(echo "$CRON_SNAP_RAW" | python3 -c \
        "import sys,json; d=json.load(sys.stdin); print(d.get('uuid',''))" 2>/dev/null || echo "")
    [ -n "$CRON_SNAP_UUID" ] \
        && _pass "cron — snapshot job created" \
        || _fail "cron — snapshot job: UUID missing" "${CRON_SNAP_RAW:0:200}"
} || _fail "cron — setSnapshotJob failed" "${CRON_SNAP_RAW:0:200}"

CRON_SCRUB_RAW=$(rpc "Zfs" "setScrubJob" "$CRON_SCRUB_PARAMS" 2>&1) && {
    CRON_SCRUB_UUID=$(echo "$CRON_SCRUB_RAW" | python3 -c \
        "import sys,json; d=json.load(sys.stdin); print(d.get('uuid',''))" 2>/dev/null || echo "")
    [ -n "$CRON_SCRUB_UUID" ] \
        && _pass "cron — scrub job created" \
        || _fail "cron — scrub job: UUID missing" "${CRON_SCRUB_RAW:0:200}"
} || _fail "cron — setScrubJob failed" "${CRON_SCRUB_RAW:0:200}"

# Deploy the cron state to write the files.
if omv-salt deploy run zfscron 2>/dev/null; then
    _pass "cron — omv-salt deploy run zfscron succeeded"
else
    _fail "cron — omv-salt deploy run zfscron failed" ""
fi

# Validate each generated cron file.
# check_cron_file <label> <file> <expected-string>
check_cron_file() {
    local label=$1 file=$2 needle=$3

    if [ ! -f "$file" ]; then
        _fail "$label — file not found: $file" ""
        return
    fi

    # Orphaned redirect: a line whose first non-whitespace token is '>' or '2>&1'.
    # This is the classic Jinja2 missing -%} whitespace bug.
    local orphan
    orphan=$(grep -nE '^\s*(>|2>&1)' "$file" || true)
    if [ -z "$orphan" ]; then
        _pass "$label — no orphaned redirect lines"
    else
        _fail "$label — redirect split onto its own line" "$orphan"
    fi

    # Every active line (not comment / blank / variable assignment) must carry
    # at least the schedule + user + command on a single line.
    # @keyword lines need ≥ 3 tokens; 5-field lines need ≥ 7 tokens.
    local bad
    bad=$(grep -vE '^\s*(#|$|[A-Z_]+=)' "$file" | while IFS= read -r line; do
        tokens=$(echo "$line" | awk '{print NF}')
        if echo "$line" | grep -qE '^\s*@'; then
            [ "$tokens" -lt 3 ] && echo "  too few tokens: $line"
        else
            [ "$tokens" -lt 7 ] && echo "  too few tokens: $line"
        fi
    done || true)
    if [ -z "$bad" ]; then
        _pass "$label — all active lines have schedule + user + command"
    else
        _fail "$label — malformed cron line(s)" "$bad"
    fi

    # The pool/dataset name must appear in the file.
    if grep -qF "$needle" "$file"; then
        _pass "$label — pool/dataset '$needle' present in file"
    else
        _fail "$label — pool/dataset '$needle' not found" "$(cat "$file")"
    fi
}

SNAP_CRON_FILE=/etc/cron.d/openmediavault-zfs-snapshots
SCRUB_CRON_FILE=/etc/cron.d/openmediavault-zfs-scrub

check_cron_file "snapshot cron" "$SNAP_CRON_FILE" "$POOL"
check_cron_file "scrub cron"    "$SCRUB_CRON_FILE" "$POOL"

# Clean up: delete jobs and redeploy to leave cron files empty.
[ -n "$CRON_SNAP_UUID"  ] && omv-rpc -u admin "Zfs" "deleteSnapshotJob" \
    "{\"uuid\":\"$CRON_SNAP_UUID\"}"  >/dev/null 2>&1 || true
[ -n "$CRON_SCRUB_UUID" ] && omv-rpc -u admin "Zfs" "deleteScrubJob" \
    "{\"uuid\":\"$CRON_SCRUB_UUID\"}" >/dev/null 2>&1 || true
omv-salt deploy run zfscron >/dev/null 2>&1 || true

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
section "Encryption — unloadEncryptionKey with child dataset mounted"
# ===========================================================================
# Regression: unload-key used to fail with "busy" when child datasets sharing
# the same encryption root were still mounted.  The fix adds -r to both the
# zfs unmount and zfs unload-key calls.

ENC_DS="$POOL/secure"
ENC_CHILD="$POOL/secure/child"
ENC_PASS="OmvZfsTestPass123!"

info "Creating encrypted dataset $ENC_DS"
# canmount=noauto prevents ZED/systemd from auto-remounting between the
# force-unmount and unload-key steps, which would cause a spurious "busy" error.
if printf '%s' "$ENC_PASS" | zfs create \
        -o encryption=aes-256-gcm \
        -o keyformat=passphrase \
        -o keylocation=prompt \
        -o canmount=noauto \
        "$ENC_DS" 2>/dev/null; then
    _pass "encrypted dataset $ENC_DS created"
else
    _fail "encrypted dataset $ENC_DS creation failed" ""
fi

info "Creating child dataset $ENC_CHILD"
if zfs create -o canmount=noauto "$ENC_CHILD" 2>/dev/null; then
    _pass "child dataset $ENC_CHILD created"
else
    _fail "child dataset $ENC_CHILD creation failed" ""
fi

# Explicitly mount both so the RPC has something to unmount.
zfs mount "$ENC_DS"   2>/dev/null || true
zfs mount "$ENC_CHILD" 2>/dev/null || true

# Confirm both are mounted before the test.
if mountpoint -q "/$ENC_DS" 2>/dev/null && mountpoint -q "/$ENC_CHILD" 2>/dev/null; then
    _pass "both datasets mounted before unload"
else
    _fail "datasets not mounted as expected before unload" "pre-condition check failed"
fi

# This is the regression case: previously failed with
# "Key unload error: '...' is busy" because the child was still mounted.
assert_rpc "unloadEncryptionKey — parent with child mounted (busy regression)" \
    "Zfs" "unloadEncryptionKey" "{\"name\":\"$ENC_DS\"}" "unloaded"

KEYSTATUS=$(zfs get -H -o value keystatus "$ENC_DS" 2>/dev/null || echo "unknown")
if [ "$KEYSTATUS" = "unavailable" ]; then
    _pass "keystatus=unavailable after unload"
else
    _fail "keystatus should be unavailable, got: $KEYSTATUS" ""
fi

if ! mountpoint -q "/$ENC_CHILD" 2>/dev/null; then
    _pass "child dataset unmounted after recursive unload"
else
    _fail "child dataset still mounted after unload" ""
fi

if ! mountpoint -q "/$ENC_DS" 2>/dev/null; then
    _pass "parent dataset unmounted after unload"
else
    _fail "parent dataset still mounted after unload" ""
fi

# Reload key for cleanup (no need to remount — destroy works with key loaded).
info "Reloading key for cleanup"
printf '%s' "$ENC_PASS" | zfs load-key "$ENC_DS" 2>/dev/null || true
zfs destroy -r "$ENC_DS" 2>/dev/null || true

# ===========================================================================
section "Clone & Promote — encrypted dataset edge cases"
# ===========================================================================
# Exercises:
#   - isencryptionroot field correctly reflects encryptionroot in listDatasets
#   - unloadEncryptionKey/loadEncryptionKey rejected for non-encryptionroot clones
#   - deleteObject on promoted clone reports the dependent dataset by name
#   - deleteObject with zfs destroy -r cleans up orphaned snapshots

ENC_SRC="$POOL/enc_src"
ENC_CLONE="$POOL/enc_clone"
ENC_CP="OmvZfsCloneTestPass456!"

info "Creating encrypted dataset $ENC_SRC"
if printf '%s' "$ENC_CP" | zfs create \
        -o encryption=aes-256-gcm \
        -o keyformat=passphrase \
        -o keylocation=prompt \
        -o canmount=noauto \
        "$ENC_SRC" 2>/dev/null; then
    _pass "enc_src — encrypted dataset created"
    zfs mount "$ENC_SRC" 2>/dev/null || true
    omv-rpc -u admin "Zfs" "doDiscover" \
        '{"addMissing":true,"deleteStale":false}' >/dev/null 2>&1 || true
else
    _fail "enc_src — encrypted dataset creation failed" ""
fi

# --- Clone enc_src via the RPC (auto-creates a snapshot) ---
CLONE_PARAMS=$(python3 -c "import json; print(json.dumps({'name':'$ENC_SRC','clonename':'enc_clone'}))")
assert_rpc "cloneDataset — clone encrypted dataset" "Zfs" "cloneDataset" "$CLONE_PARAMS"
zfs mount "$ENC_CLONE" 2>/dev/null || true

# --- isencryptionroot before promote ---
ENC_CLONE_MP=$(zfs get -H -o value mountpoint "$ENC_CLONE" 2>/dev/null || echo "")
ENC_SRC_MP=$(zfs get -H -o value mountpoint   "$ENC_SRC"   2>/dev/null || echo "")

DS_LIST=$(omv-rpc -u admin "Zfs" "listDatasets" \
    '{"start":0,"limit":null,"sortfield":null,"sortdir":null}' 2>/dev/null || echo "[]")

_enc_is_root() {
    # $1 = path; echos true/false/missing/not_found
    # listDatasets returns {"data":[...],"total":N} via applyFilter
    echo "$DS_LIST" | python3 -c "
import sys, json
raw = json.load(sys.stdin)
items = raw['data'] if isinstance(raw, dict) and 'data' in raw else raw
obj = next((o for o in items if o.get('path') == '$1'), None)
print(str(obj.get('isencryptionroot', 'missing')).lower() if obj else 'not_found')
" 2>/dev/null || echo "error"
}

SRC_IS_ROOT=$(_enc_is_root "$ENC_SRC")
CLONE_IS_ROOT=$(_enc_is_root "$ENC_CLONE")

if [ "$SRC_IS_ROOT" = "true" ]; then
    _pass "isencryptionroot — enc_src is true (is the encryption root)"
else
    _fail "isencryptionroot — enc_src should be true, got: $SRC_IS_ROOT" ""
fi
if [ "$CLONE_IS_ROOT" = "false" ]; then
    _pass "isencryptionroot — enc_clone is false (inherits enc_src as root)"
else
    _fail "isencryptionroot — enc_clone should be false, got: $CLONE_IS_ROOT" ""
fi

# --- unloadEncryptionKey on non-root clone must be rejected with clear error ---
UNLOAD_OUT=$(omv-rpc -u admin "Zfs" "unloadEncryptionKey" \
    "{\"name\":\"$ENC_CLONE\"}" 2>&1) || true
if echo "$UNLOAD_OUT" | grep -qi "encryption root\|encryptionroot\|independently"; then
    _pass "unloadEncryptionKey — non-root clone rejected with encryptionroot error"
else
    _fail "unloadEncryptionKey — expected encryptionroot rejection for non-root clone" \
          "${UNLOAD_OUT:0:300}"
fi
# Verify enc_src key was NOT unloaded (the guard must fire before any side effects).
KEYSTATUS_SRC=$(zfs get -H -o value keystatus "$ENC_SRC" 2>/dev/null || echo "unknown")
if [ "$KEYSTATUS_SRC" = "available" ]; then
    _pass "unloadEncryptionKey — rejection did not affect enc_src key"
else
    _fail "unloadEncryptionKey — enc_src key should still be available, got: $KEYSTATUS_SRC" ""
fi

# --- loadEncryptionKey on non-root clone must be rejected ---
# First lock the real encryptionroot, then try to unlock via the clone.
# The unloadEncryptionKey RPC unmounts all datasets sharing this encryptionroot
# (including sibling clones) before calling zfs unload-key, so no manual
# unmount is required here.
assert_rpc "unloadEncryptionKey — lock the real root (enc_src) before clone-unlock test" \
    "Zfs" "unloadEncryptionKey" "{\"name\":\"$ENC_SRC\"}" "unloaded"

LOAD_PARAMS=$(python3 -c "import json; print(json.dumps({'name':'$ENC_CLONE','key':'$ENC_CP'}))")
LOAD_OUT=$(omv-rpc -u admin "Zfs" "loadEncryptionKey" "$LOAD_PARAMS" 2>&1) || true
if echo "$LOAD_OUT" | grep -qi "encryption root\|encryptionroot\|independently"; then
    _pass "loadEncryptionKey — non-root clone rejected with encryptionroot error"
else
    _fail "loadEncryptionKey — expected encryptionroot rejection for non-root clone" \
          "${LOAD_OUT:0:300}"
fi

# Re-unlock enc_src (the real root) to continue.
RELOAD_PARAMS=$(python3 -c "import json; print(json.dumps({'name':'$ENC_SRC','key':'$ENC_CP'}))")
assert_rpc "loadEncryptionKey — re-unlock the real root (enc_src)" \
    "Zfs" "loadEncryptionKey" "$RELOAD_PARAMS" "loaded"

# --- Promote enc_clone; enc_clone becomes the new encryptionroot ---
assert_rpc "promoteDataset — promote enc_clone" \
    "Zfs" "promoteDataset" "{\"name\":\"$ENC_CLONE\"}"

sleep 1
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":true,"deleteStale":false}' >/dev/null 2>&1 || true

DS_LIST=$(omv-rpc -u admin "Zfs" "listDatasets" \
    '{"start":0,"limit":null,"sortfield":null,"sortdir":null}' 2>/dev/null || echo "[]")
POST_SRC_IS_ROOT=$(_enc_is_root "$ENC_SRC")
POST_CLONE_IS_ROOT=$(_enc_is_root "$ENC_CLONE")

if [ "$POST_CLONE_IS_ROOT" = "true" ]; then
    _pass "isencryptionroot after promote — enc_clone is now the encryption root"
else
    _fail "isencryptionroot after promote — enc_clone should be true, got: $POST_CLONE_IS_ROOT" ""
fi
if [ "$POST_SRC_IS_ROOT" = "false" ]; then
    _pass "isencryptionroot after promote — enc_src is no longer the encryption root"
else
    _fail "isencryptionroot after promote — enc_src should be false, got: $POST_SRC_IS_ROOT" ""
fi

# --- deleteObject on promoted enc_clone must fail: its snapshot is enc_src's origin ---
DEL_CLONE_OUT=$(omv-rpc -u admin "Zfs" "deleteObject" \
    "{\"name\":\"$ENC_CLONE\",\"mp\":\"$ENC_CLONE_MP\",\"type\":\"Filesystem\"}" 2>&1) || true
if echo "$DEL_CLONE_OUT" | grep -qi "origin\|dependent\|clone\|delete.*first"; then
    _pass "deleteObject — promoted clone deletion rejected: snapshot has dependents"
else
    _fail "deleteObject — expected dependent-clone error when deleting promoted clone" \
          "${DEL_CLONE_OUT:0:300}"
fi
if echo "$DEL_CLONE_OUT" | sed 's|\\/|/|g' | grep -qF "$ENC_SRC"; then
    _pass "deleteObject — error names the dependent dataset ($ENC_SRC)"
else
    _fail "deleteObject — error should name the dependent ($ENC_SRC)" \
          "${DEL_CLONE_OUT:0:300}"
fi

# Verify enc_clone still exists (deletion must have been fully aborted).
if zfs list -H -o name "$ENC_CLONE" >/dev/null 2>&1; then
    _pass "deleteObject — enc_clone still exists (delete was aborted)"
else
    _fail "deleteObject — enc_clone was destroyed despite having dependents" ""
fi

# --- Delete enc_src first (now the clone, no snapshot dependents) ---
assert_rpc "deleteObject — enc_src (now the dependent clone, delete first)" \
    "Zfs" "deleteObject" \
    "{\"name\":\"$ENC_SRC\",\"mp\":\"$ENC_SRC_MP\",\"type\":\"Filesystem\"}"
if ! zfs list -H -o name "$ENC_SRC" >/dev/null 2>&1; then
    _pass "deleteObject — enc_src destroyed successfully"
else
    _fail "deleteObject — enc_src still exists after delete" ""
fi

# --- Now enc_clone has an orphaned snapshot with no dependents.
#     deleteObject must succeed, using zfs destroy -r to remove the snapshot too. ---
assert_rpc "deleteObject — enc_clone (promoted, orphaned snapshot cleaned by -r)" \
    "Zfs" "deleteObject" \
    "{\"name\":\"$ENC_CLONE\",\"mp\":\"$ENC_CLONE_MP\",\"type\":\"Filesystem\"}"
if ! zfs list -H -o name "$ENC_CLONE" >/dev/null 2>&1; then
    _pass "deleteObject — enc_clone and its orphaned snapshot destroyed"
else
    _fail "deleteObject — enc_clone still exists after delete" ""
fi

unset ENC_SRC ENC_CLONE ENC_CLONE_MP ENC_SRC_MP ENC_CP
unset CLONE_PARAMS DS_LIST SRC_IS_ROOT CLONE_IS_ROOT
unset UNLOAD_OUT LOAD_PARAMS LOAD_OUT RELOAD_PARAMS
unset POST_SRC_IS_ROOT POST_CLONE_IS_ROOT DEL_CLONE_OUT

# ===========================================================================
section "Encryption — auto-unlock lifecycle"
# ===========================================================================
# Simulates the user workflow: enable encryption with auto-unlock, disable
# auto-unlock (removeAutoUnlock), then simulate a reboot by manually locking
# the dataset (unload-key) and re-unlocking via loadEncryptionKey.
#
# Also tests that doDiscover (deleteStale=true) does NOT remove the mntent
# entry for a locked (unmounted) encrypted dataset — the real-world risk when
# auto-unlock is off and the dataset isn't mounted at boot.

AU_DS="$POOL/au_test"
AU_PASS="AutoUnlockTestPass789!"

info "Creating encrypted dataset $AU_DS via enableEncryption RPC (autounlock=true)"
AU_ENC_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'path':           '$POOL',
    'name':           'au_test',
    'encryptiontype': 'aes-256-gcm',
    'key':            '$AU_PASS',
    'autounlock':     True,
}))
")
if omv-rpc -u admin "Zfs" "enableEncryption" "$AU_ENC_PARAMS" >/dev/null 2>&1; then
    _pass "enableEncryption — $AU_DS created with autounlock=true"
else
    _fail "enableEncryption — $AU_DS creation failed" ""
fi

# Verify the dataset exists and is mounted.
if zfs list -H -o name "$AU_DS" >/dev/null 2>&1; then
    _pass "enableEncryption — $AU_DS exists in ZFS"
else
    _fail "enableEncryption — $AU_DS not found in ZFS" ""
fi

AU_MP=$(zfs get -H -o value mountpoint "$AU_DS" 2>/dev/null || echo "")
if [ -n "$AU_MP" ] && [ "$AU_MP" != "none" ] && [ "$AU_MP" != "-" ] && mountpoint -q "$AU_MP" 2>/dev/null; then
    _pass "enableEncryption — $AU_DS is mounted at $AU_MP (CASE 2 regression)"
else
    _fail "enableEncryption — $AU_DS not mounted after RPC (CASE 2 regression)" \
          "enableEncryption must mount the new dataset; got mounted=$(zfs get -H -o value mounted "$AU_DS" 2>/dev/null)"
fi

# --- CASE 2 regression: addObject child filesystem on encrypted parent -------
# Creating a filesystem whose parent is an encrypted dataset must also result
# in the new dataset being mounted immediately, not just created.
AU_CHILD="$POOL/au_test/subfs"
assert_rpc "addObject — filesystem child of encrypted dataset (CASE 2)" \
    "Zfs" "addObject" \
    "{\"type\":\"filesystem\",\"path\":\"$POOL/au_test\",\"name\":\"subfs\",\"mountpoint\":\"\",\"compression\":\"\",\"recordsize\":\"\"}"
AU_CHILD_MOUNTED=$(zfs get -H -o value mounted "$AU_CHILD" 2>/dev/null || echo "no")
if [ "$AU_CHILD_MOUNTED" = "yes" ]; then
    _pass "addObject — child of encrypted dataset mounted (CASE 2 regression)"
else
    _fail "addObject — child of encrypted dataset not mounted (CASE 2 regression)" \
          "Expected mounted=yes after addObject, got: $AU_CHILD_MOUNTED"
fi

# Verify getEncryptionStatus reports autounlock=true and keystatus=available.
AU_STATUS=$(omv-rpc -u admin "Zfs" "getEncryptionStatus" \
    "{\"name\":\"$AU_DS\"}" 2>/dev/null || echo "{}")
AU_AUTOUNLOCK=$(echo "$AU_STATUS" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(str(d.get('autounlock','')).lower())" \
    2>/dev/null || echo "")
AU_KEYSTATUS=$(echo "$AU_STATUS" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(d.get('keystatus',''))" \
    2>/dev/null || echo "")
if [ "$AU_AUTOUNLOCK" = "true" ]; then
    _pass "getEncryptionStatus — autounlock=true after enableEncryption"
else
    _fail "getEncryptionStatus — autounlock should be true, got: $AU_AUTOUNLOCK" ""
fi
if [ "$AU_KEYSTATUS" = "available" ]; then
    _pass "getEncryptionStatus — keystatus=available (key loaded)"
else
    _fail "getEncryptionStatus — keystatus should be available, got: $AU_KEYSTATUS" ""
fi

# ---- Disable auto-unlock (removeAutoUnlock) --------------------------------
assert_rpc "removeAutoUnlock — disable auto-unlock on $AU_DS" \
    "Zfs" "removeAutoUnlock" "{\"name\":\"$AU_DS\"}"

# Keyfile must be gone.
# getKeyfilePath uses str_replace('/', '_', $name), so e.g. pool/au_test → pool_au_test.key
AU_KEYFILE="/etc/zfs/keys/$(echo "$AU_DS" | tr '/' '_').key"
if [ -f "$AU_KEYFILE" ]; then
    _fail "removeAutoUnlock — keyfile still present: $AU_KEYFILE" ""
else
    _pass "removeAutoUnlock — keyfile removed from /etc/zfs/keys/"
fi

# getEncryptionStatus must now report autounlock=false, keylocation=prompt.
AU_STATUS2=$(omv-rpc -u admin "Zfs" "getEncryptionStatus" \
    "{\"name\":\"$AU_DS\"}" 2>/dev/null || echo "{}")
AU_AUTOUNLOCK2=$(echo "$AU_STATUS2" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(str(d.get('autounlock','')).lower())" \
    2>/dev/null || echo "")
AU_KEYLOC2=$(echo "$AU_STATUS2" | python3 -c \
    "import sys,json; d=json.load(sys.stdin); print(d.get('keylocation',''))" \
    2>/dev/null || echo "")
if [ "$AU_AUTOUNLOCK2" = "false" ]; then
    _pass "getEncryptionStatus — autounlock=false after removeAutoUnlock"
else
    _fail "getEncryptionStatus — autounlock should be false, got: $AU_AUTOUNLOCK2" ""
fi
if [ "$AU_KEYLOC2" = "prompt" ]; then
    _pass "getEncryptionStatus — keylocation=prompt after removeAutoUnlock"
else
    _fail "getEncryptionStatus — keylocation should be prompt, got: $AU_KEYLOC2" ""
fi

# ---- removeAutoUnlock must mask the zfs-load-key@ unit ---------------------
# The mask (/dev/null symlink in /etc/systemd/system/) prevents zfs-mount-generator
# from prompting for a passphrase at boot when auto-unlock is disabled.
AU_INSTANCE=$(systemd-escape -p "$AU_DS" 2>/dev/null || echo "")
AU_MASK_FILE="/etc/systemd/system/zfs-load-key@${AU_INSTANCE}.service"
if [ -n "$AU_INSTANCE" ]; then
    if [ -L "$AU_MASK_FILE" ] && [ "$(readlink "$AU_MASK_FILE")" = "/dev/null" ]; then
        _pass "removeAutoUnlock — zfs-load-key@${AU_INSTANCE}.service masked (/dev/null symlink)"
    else
        _fail "removeAutoUnlock — zfs-load-key@${AU_INSTANCE}.service not masked" \
              "expected $AU_MASK_FILE -> /dev/null; $(ls -la "$AU_MASK_FILE" 2>/dev/null || echo 'file not found')"
    fi
fi

# ---- wait-for-remote-unlock.conf drop-in state must match active remote keys ---
# The drop-in is managed dynamically: present when any dataset has remote unlock
# configured, absent when none do.  Other datasets on the system may legitimately
# have remote unlock active, so we check the actual json count rather than assuming
# the test environment is clean.
AU_DROPIN="/etc/systemd/system/zfs-load-key@.service.d/wait-for-remote-unlock.conf"
AU_REMOTE_KEY_COUNT=$(ls /etc/zfs/remote-keys/*.json 2>/dev/null | wc -l)
if [ "$AU_REMOTE_KEY_COUNT" -eq 0 ]; then
    if [ ! -f "$AU_DROPIN" ]; then
        _pass "manageRemoteUnlockService — drop-in absent (no remote keys configured)"
    else
        _fail "manageRemoteUnlockService — drop-in present but no remote keys exist" \
              "drop-in should only exist when at least one dataset has remote unlock active"
    fi
else
    if [ -f "$AU_DROPIN" ]; then
        _pass "manageRemoteUnlockService — drop-in present ($AU_REMOTE_KEY_COUNT other remote key(s) configured)"
    else
        _fail "manageRemoteUnlockService — drop-in absent despite $AU_REMOTE_KEY_COUNT remote key(s) existing" \
              "drop-in should be present when any dataset has remote unlock active"
    fi
fi

# ---- Simulate reboot: lock the dataset (unload-key) ------------------------
# After disabling auto-unlock and rebooting, ZFS cannot load the key
# automatically (keylocation=prompt).  We simulate this by locking the
# dataset now via unloadEncryptionKey.
assert_rpc "unloadEncryptionKey — simulate post-reboot locked state" \
    "Zfs" "unloadEncryptionKey" "{\"name\":\"$AU_DS\"}" "unloaded"

AU_KEYSTATUS2=$(zfs get -H -o value keystatus "$AU_DS" 2>/dev/null || echo "unknown")
if [ "$AU_KEYSTATUS2" = "unavailable" ]; then
    _pass "simulate reboot — keystatus=unavailable (dataset locked, as after boot without auto-unlock)"
else
    _fail "simulate reboot — keystatus should be unavailable, got: $AU_KEYSTATUS2" ""
fi

# ---- setAutoUnlock must unmask the unit (tested while dataset is locked) ----
# The round-trip is done here — after unloadEncryptionKey while datasets are
# unmounted — so that the daemon-reload inside setAutoUnlock/removeAutoUnlock
# does not interfere with the mount tracking of live filesystems.
AU_SET_PARAMS=$(python3 -c "
import json
print(json.dumps({'name': '$AU_DS', 'keyloctype': 'local', 'key': '$AU_PASS'}))
")
assert_rpc "setAutoUnlock — re-enable local auto-unlock on $AU_DS (dataset locked)" \
    "Zfs" "setAutoUnlock" "$AU_SET_PARAMS"

if [ -n "$AU_INSTANCE" ]; then
    if [ -L "$AU_MASK_FILE" ] && [ "$(readlink "$AU_MASK_FILE")" = "/dev/null" ]; then
        _fail "setAutoUnlock — zfs-load-key@${AU_INSTANCE}.service still masked after re-enable" ""
    else
        _pass "setAutoUnlock — zfs-load-key@${AU_INSTANCE}.service unmasked after re-enable"
    fi
fi
if [ -f "$AU_KEYFILE" ]; then
    _pass "setAutoUnlock — keyfile re-created at $AU_KEYFILE"
else
    _fail "setAutoUnlock — keyfile not found at $AU_KEYFILE after re-enable" ""
fi

# Disable auto-unlock again (re-masks the unit) before the manual-unlock test.
assert_rpc "removeAutoUnlock — disable again before manual-unlock test" \
    "Zfs" "removeAutoUnlock" "{\"name\":\"$AU_DS\"}"
if [ -n "$AU_INSTANCE" ]; then
    if [ -L "$AU_MASK_FILE" ] && [ "$(readlink "$AU_MASK_FILE")" = "/dev/null" ]; then
        _pass "removeAutoUnlock (2nd) — zfs-load-key@${AU_INSTANCE}.service re-masked"
    else
        _fail "removeAutoUnlock (2nd) — zfs-load-key@${AU_INSTANCE}.service not masked" ""
    fi
fi


# ---- doDiscover(deleteStale=true) must NOT remove the mntent entry ---------
# This is the subtle bug: a locked dataset is unmounted, so its mountpoint
# looks "stale" to a naive discover pass.  The plugin must recognise encrypted
# locked datasets and keep their mntent entries intact.
if [ "$(mntent_exists "$AU_DS")" = "True" ]; then
    info "mntent entry present before discover (expected)"
    assert_rpc_bg "doDiscoverBg — deleteStale=true with locked encrypted dataset" \
        "Zfs" "doDiscoverBg" '{"addMissing":false,"deleteStale":true}'
    if [ "$(mntent_exists "$AU_DS")" = "True" ]; then
        _pass "doDiscover(deleteStale) — mntent entry preserved for locked encrypted dataset"
    else
        _fail "doDiscover(deleteStale) — mntent entry incorrectly removed for locked dataset" \
              "Files still exist in ZFS but OMV no longer tracks the filesystem"
    fi
else
    info "Skipping doDiscover(deleteStale) check — no mntent entry found (enableEncryption may not have registered one)"
fi

# ---- Manually unlock (simulating user action after reboot) -----------------
AU_LOAD_PARAMS=$(python3 -c "
import json
print(json.dumps({'name': '$AU_DS', 'key': '$AU_PASS'}))
")
assert_rpc "loadEncryptionKey — manually unlock after simulated reboot" \
    "Zfs" "loadEncryptionKey" "$AU_LOAD_PARAMS" "loaded"

AU_KEYSTATUS3=$(zfs get -H -o value keystatus "$AU_DS" 2>/dev/null || echo "unknown")
if [ "$AU_KEYSTATUS3" = "available" ]; then
    _pass "loadEncryptionKey — keystatus=available after manual unlock"
else
    _fail "loadEncryptionKey — keystatus should be available, got: $AU_KEYSTATUS3" ""
fi
# The dataset must also be mounted, not just unlocked (CASE 1 regression).
AU_MOUNTED3=$(zfs get -H -o value mounted "$AU_DS" 2>/dev/null || echo "no")
if [ "$AU_MOUNTED3" = "yes" ]; then
    _pass "loadEncryptionKey — $AU_DS is mounted after unlock (CASE 1 regression)"
else
    _fail "loadEncryptionKey — $AU_DS not mounted after unlock (CASE 1 regression)" \
          "keystatus=$AU_KEYSTATUS3 but mounted=$AU_MOUNTED3"
fi
# Child datasets sharing the encryptionroot must also remount.
# Regression: old code only called addOMVMntEntForDataset on the root dataset,
# and the mount loop swallowed errors, so children could stay unmounted while
# loadEncryptionKey still returned success.
AU_CHILD_MOUNTED3=$(zfs get -H -o value mounted "$AU_CHILD" 2>/dev/null || echo "no")
if [ "$AU_CHILD_MOUNTED3" = "yes" ]; then
    _pass "loadEncryptionKey — $AU_CHILD (child) is mounted after unlock"
else
    _fail "loadEncryptionKey — $AU_CHILD (child) not mounted after unlock" \
          "loadEncryptionKey must remount all datasets sharing the encryptionroot; got mounted=$AU_CHILD_MOUNTED3"
fi

# ---- Cleanup ---------------------------------------------------------------
info "Cleaning up $AU_DS"
# Use -f (force) to unmount before destroying — the dataset may still be
# mounted if the test exercised the direct-mount fallback path.
zfs destroy -rf "$AU_DS" 2>/dev/null || true
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":false,"deleteStale":true}' >/dev/null 2>&1 || true

unset AU_DS AU_PASS AU_ENC_PARAMS AU_MP
unset AU_CHILD AU_CHILD_MOUNTED AU_CHILD_MOUNTED3
unset AU_STATUS AU_STATUS2 AU_AUTOUNLOCK AU_AUTOUNLOCK2
unset AU_KEYSTATUS AU_KEYSTATUS2 AU_KEYSTATUS3 AU_KEYLOC2
unset AU_MOUNTED3 AU_LOAD_PARAMS AU_KEYFILE

# ===========================================================================
section "Encryption — loadEncryptionKey with sub-encryption-root in hierarchy (CASE 1)"
# ===========================================================================
# Regression test for the bug where zfs mount -R failed when the hierarchy
# contained a child dataset that is its own encryption root (unavailable key).
# The failure caused ALL datasets — including those that could be mounted — to
# remain unmounted, because zfsTryExec swallowed the error and the sibling loop
# skipped all children assuming -R had handled them.
#
# Layout:
#   $POOL/enc_hier          — encroot=self (PASS1)
#   $POOL/enc_hier/inherit  — inherits enc_hier key (should mount with enc_hier)
#   $POOL/enc_hier/sub_enc  — own encroot (PASS2, different key, stays locked)

EH_DS="$POOL/enc_hier"
EH_CHILD="$POOL/enc_hier/inherit"
EH_SUB="$POOL/enc_hier/sub_enc"
EH_PASS1="HierPass111!"
EH_PASS2="SubEncPass222!"

info "Creating $EH_DS (own encroot, pass1)"
if zfs create \
        -o encryption=aes-256-gcm \
        -o keyformat=passphrase \
        -o keylocation=prompt \
        -o canmount=noauto \
        "$EH_DS" <<< "$EH_PASS1" 2>/dev/null; then
    _pass "enc_hier — $EH_DS created"
    zfs mount "$EH_DS" 2>/dev/null || true
else
    _fail "enc_hier — $EH_DS creation failed (skipping section)" ""
    EH_DS=""
fi

if [ -n "$EH_DS" ]; then
    info "Creating $EH_CHILD (inherits enc_hier key)"
    if zfs create -o canmount=noauto "$EH_CHILD" 2>/dev/null; then
        _pass "enc_hier — $EH_CHILD created"
        zfs mount "$EH_CHILD" 2>/dev/null || true
    else
        _fail "enc_hier — $EH_CHILD creation failed" ""
    fi

    info "Creating $EH_SUB (own encroot, pass2 — different key)"
    if zfs create \
            -o encryption=aes-256-gcm \
            -o keyformat=passphrase \
            -o keylocation=prompt \
            -o canmount=noauto \
            "$EH_SUB" <<< "$EH_PASS2" 2>/dev/null; then
        _pass "enc_hier — $EH_SUB created (separate encroot)"
    else
        _fail "enc_hier — $EH_SUB creation failed" ""
    fi

    # Lock enc_hier (and its inherit child).  sub_enc has its own key — also
    # unload that so both enc roots start unavailable.
    zfs unmount "$EH_CHILD" 2>/dev/null || true
    zfs unmount "$EH_DS"    2>/dev/null || true
    zfs unmount "$EH_SUB"   2>/dev/null || true
    zfs unload-key "$EH_SUB" 2>/dev/null || true
    zfs unload-key "$EH_DS"  2>/dev/null || true

    EH_KS=$(zfs get -H -o value keystatus "$EH_DS" 2>/dev/null || echo "unknown")
    if [ "$EH_KS" = "unavailable" ]; then
        _pass "enc_hier — $EH_DS locked (pre-condition)"
    else
        _fail "enc_hier — expected $EH_DS locked, got keystatus=$EH_KS" ""
    fi

    # Call loadEncryptionKey for enc_hier only (not sub_enc).
    EH_LOAD_PARAMS=$(python3 -c "import json; print(json.dumps({'name': '$EH_DS', 'key': '$EH_PASS1'}))")
    assert_rpc "loadEncryptionKey — enc_hier (hierarchy with sub-encroot, CASE 1)" \
        "Zfs" "loadEncryptionKey" "$EH_LOAD_PARAMS" "loaded"

    # enc_hier and its inherit child must be mounted.
    EH_MOUNTED=$(zfs get -H -o value mounted "$EH_DS" 2>/dev/null || echo "no")
    if [ "$EH_MOUNTED" = "yes" ]; then
        _pass "loadEncryptionKey — enc_hier root mounted after unlock (CASE 1 regression)"
    else
        _fail "loadEncryptionKey — enc_hier root NOT mounted (CASE 1 regression)" \
              "zfs mount -R failed silently on sub-encroot; expected mounted=yes"
    fi

    EH_CHILD_MOUNTED=$(zfs get -H -o value mounted "$EH_CHILD" 2>/dev/null || echo "no")
    if [ "$EH_CHILD_MOUNTED" = "yes" ]; then
        _pass "loadEncryptionKey — enc_hier/inherit mounted after unlock (CASE 1 regression)"
    else
        _fail "loadEncryptionKey — enc_hier/inherit NOT mounted (CASE 1 regression)" \
              "child inheriting key should be mounted; got mounted=$EH_CHILD_MOUNTED"
    fi

    # sub_enc must remain unmounted — its key was not loaded.
    EH_SUB_MOUNTED=$(zfs get -H -o value mounted "$EH_SUB" 2>/dev/null || echo "no")
    if [ "$EH_SUB_MOUNTED" = "no" ]; then
        _pass "loadEncryptionKey — enc_hier/sub_enc correctly NOT mounted (own key not loaded)"
    else
        _fail "loadEncryptionKey — enc_hier/sub_enc should not be mounted (own key not loaded)" \
              "got mounted=$EH_SUB_MOUNTED"
    fi

    # Cleanup
    zfs unload-key -r "$EH_DS" 2>/dev/null || true
    zfs unload-key    "$EH_SUB" 2>/dev/null || true
    zfs destroy -rf "$EH_DS" 2>/dev/null || true
fi
unset EH_DS EH_CHILD EH_SUB EH_PASS1 EH_PASS2 EH_LOAD_PARAMS
unset EH_KS EH_MOUNTED EH_CHILD_MOUNTED EH_SUB_MOUNTED

# ===========================================================================
section "Encryption — loadEncryptionKey with non-mountable datasets"
# ===========================================================================
# Regression: before the fix, loadEncryptionKey scanned ALL filesystems sharing
# the encryptionroot and required every one to reach mounted=yes.  Datasets with
# canmount=off or mountpoint=none legitimately share an encryptionroot but
# cannot be mounted, so the RPC falsely threw "failed to mount" even though the
# key loaded and the mountable datasets came up fine.
#
# Layout:
#   $POOL/enc_nonmount         — encroot=self
#   $POOL/enc_nonmount/normal  — inherits key, standard mountpoint (must mount)
#   $POOL/enc_nonmount/canoff  — inherits key, canmount=off (must NOT cause failure)
#   $POOL/enc_nonmount/nomp    — inherits key, mountpoint=none (must NOT cause failure)

NM_DS="$POOL/enc_nonmount"
NM_PASS="NonMountPass111!"
NM_NORMAL="$NM_DS/normal"
NM_CANOFF="$NM_DS/canoff"
NM_NOMP="$NM_DS/nomp"

info "Creating root encrypted dataset $NM_DS"
if printf '%s' "$NM_PASS" | zfs create \
        -o encryption=aes-256-gcm \
        -o keyformat=passphrase \
        -o keylocation=prompt \
        "$NM_DS" 2>/dev/null; then
    _pass "enc_nonmount — root dataset created"
    zfs create              "$NM_NORMAL" 2>/dev/null \
        && _pass "enc_nonmount — normal child created" \
        || _fail "enc_nonmount — normal child creation failed" ""
    zfs create -o canmount=off   "$NM_CANOFF" 2>/dev/null \
        && _pass "enc_nonmount — canmount=off child created" \
        || _fail "enc_nonmount — canmount=off child creation failed" ""
    zfs create -o mountpoint=none "$NM_NOMP"  2>/dev/null \
        && _pass "enc_nonmount — mountpoint=none child created" \
        || _fail "enc_nonmount — mountpoint=none child creation failed" ""

    # Lock all datasets sharing this encryptionroot.
    zfs unmount -f "$NM_NORMAL" 2>/dev/null || true
    zfs unmount -f "$NM_DS"     2>/dev/null || true
    zfs unload-key -r "$NM_DS"  2>/dev/null || true

    NM_KS=$(zfs get -H -o value keystatus "$NM_DS" 2>/dev/null || echo "unknown")
    if [ "$NM_KS" = "unavailable" ]; then
        _pass "enc_nonmount — dataset locked (keystatus=unavailable)"
    else
        _fail "enc_nonmount — expected locked dataset, got keystatus=$NM_KS" ""
    fi

    # Unlock via RPC.  Must succeed despite canmount=off and mountpoint=none children.
    NM_LOAD_PARAMS=$(python3 -c "import json; print(json.dumps({'name': '$NM_DS', 'key': '$NM_PASS'}))")
    assert_rpc "loadEncryptionKey — non-mountable datasets must not cause false failure" \
        "Zfs" "loadEncryptionKey" "$NM_LOAD_PARAMS" "loaded"

    # Normal child must be mounted.
    NM_NORMAL_MOUNTED=$(zfs get -H -o value mounted "$NM_NORMAL" 2>/dev/null || echo "no")
    if [ "$NM_NORMAL_MOUNTED" = "yes" ]; then
        _pass "loadEncryptionKey — normal child mounted after unlock"
    else
        _fail "loadEncryptionKey — normal child not mounted after unlock" \
              "got mounted=$NM_NORMAL_MOUNTED"
    fi

    # canmount=off child must remain unmounted (by design, not a failure).
    NM_CANOFF_MOUNTED=$(zfs get -H -o value mounted "$NM_CANOFF" 2>/dev/null || echo "no")
    if [ "$NM_CANOFF_MOUNTED" = "no" ]; then
        _pass "loadEncryptionKey — canmount=off child correctly not mounted"
    else
        _fail "loadEncryptionKey — canmount=off child should not be mounted" \
              "got mounted=$NM_CANOFF_MOUNTED"
    fi

    # mountpoint=none child must remain unmounted (by design, not a failure).
    NM_NOMP_MOUNTED=$(zfs get -H -o value mounted "$NM_NOMP" 2>/dev/null || echo "no")
    if [ "$NM_NOMP_MOUNTED" = "no" ]; then
        _pass "loadEncryptionKey — mountpoint=none child correctly not mounted"
    else
        _fail "loadEncryptionKey — mountpoint=none child should not be mounted" \
              "got mounted=$NM_NOMP_MOUNTED"
    fi
else
    _fail "enc_nonmount — root dataset creation failed" ""
fi

zfs destroy -rf "$NM_DS" 2>/dev/null || true
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":false,"deleteStale":true}' >/dev/null 2>&1 || true
unset NM_DS NM_PASS NM_NORMAL NM_CANOFF NM_NOMP NM_LOAD_PARAMS
unset NM_KS NM_NORMAL_MOUNTED NM_CANOFF_MOUNTED NM_NOMP_MOUNTED

# ===========================================================================
section "Encryption — addOMVMntEntForDataset repairs stale dir"
# ===========================================================================
# Regression: before the fix, if an OMV mntent entry already existed for a
# dataset, addOMVMntEntForDataset returned immediately without checking whether
# the stored dir matched the current ZFS mountpoint.  After a CLI mountpoint
# change the entry would remain stale indefinitely.
#
# This test changes the mountpoint via CLI (bypassing OMV), then triggers
# addOMVMntEntForDataset via loadEncryptionKey and verifies the dir is repaired.

SD_DS="$POOL/stale_dir_test"
SD_PASS="StaleDirPass222!"
SD_NEW_MP="/${POOL}_sd_new"

SD_ENC_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'path':           '$POOL',
    'name':           'stale_dir_test',
    'encryptiontype': 'aes-256-gcm',
    'key':            '$SD_PASS',
    'autounlock':     False,
}))
")
if omv-rpc -u admin "Zfs" "enableEncryption" "$SD_ENC_PARAMS" >/dev/null 2>&1; then
    _pass "stale_dir — $SD_DS created via enableEncryption"
else
    _fail "stale_dir — $SD_DS creation failed" ""
fi

if zfs list -H -o name "$SD_DS" >/dev/null 2>&1; then
    SD_ORIG_MP=$(zfs get -H -o value mountpoint "$SD_DS" 2>/dev/null || echo "")

    # Verify the mntent entry was registered with the correct original mountpoint.
    SD_OMV_DIR=$(mntent_dir "$SD_DS")
    if [ "$SD_OMV_DIR" = "$SD_ORIG_MP" ]; then
        _pass "stale_dir — initial OMV dir matches ZFS mountpoint ($SD_ORIG_MP)"
    else
        _fail "stale_dir — initial OMV dir mismatch: OMV='$SD_OMV_DIR' ZFS='$SD_ORIG_MP'" ""
    fi

    # Change the mountpoint via CLI, bypassing OMV — this creates a stale entry.
    zfs unmount "$SD_DS"                        2>/dev/null || true
    zfs set "mountpoint=$SD_NEW_MP" "$SD_DS"    2>/dev/null || true
    zfs mount "$SD_DS"                          2>/dev/null || true

    # Lock the dataset so loadEncryptionKey has something to do.
    zfs unmount -f "$SD_DS"    2>/dev/null || true
    zfs unload-key  "$SD_DS"   2>/dev/null || true

    # Unlock via RPC — calls addOMVMntEntForDataset, which must repair the dir.
    SD_LOAD_PARAMS=$(python3 -c "import json; print(json.dumps({'name': '$SD_DS', 'key': '$SD_PASS'}))")
    assert_rpc "stale_dir — loadEncryptionKey triggers addOMVMntEntForDataset" \
        "Zfs" "loadEncryptionKey" "$SD_LOAD_PARAMS" "loaded"

    SD_OMV_DIR2=$(mntent_dir "$SD_DS")
    if [ "$SD_OMV_DIR2" = "$SD_NEW_MP" ]; then
        _pass "stale_dir — OMV dir updated to new mountpoint ($SD_NEW_MP)"
    else
        _fail "stale_dir — OMV dir not updated: expected '$SD_NEW_MP', got '$SD_OMV_DIR2'" ""
    fi
fi

zfs destroy -rf "$SD_DS" 2>/dev/null || true
rmdir "$SD_NEW_MP" 2>/dev/null || true
omv-rpc -u admin "Zfs" "doDiscover" \
    '{"addMissing":false,"deleteStale":true}' >/dev/null 2>&1 || true
unset SD_DS SD_PASS SD_NEW_MP SD_ENC_PARAMS SD_ORIG_MP
unset SD_OMV_DIR SD_OMV_DIR2 SD_LOAD_PARAMS

# ===========================================================================
section "Filesystem deletion — zfs-list.cache and systemd unit cleanup"
# ===========================================================================
# Verifies that deleteObject:
#   1. Removes the deleted dataset from /etc/zfs/zfs-list.cache/<pool>
#   2. Stops and clears the zfs-load-key@ service for the dataset
#
# Uses an encrypted auto-unlock dataset so the cache has a keylocation=file://
# entry and zfs-mount-generator generates the corresponding service unit.

ZLC_CACHE="/etc/zfs/zfs-list.cache/$POOL"
ZLC_DS="$POOL/zlc_test"
ZLC_PASS="ZlcTestPass321!"

info "Creating encrypted dataset $ZLC_DS with auto-unlock"
ZLC_ENC_PARAMS=$(python3 -c "
import json
print(json.dumps({
    'path':           '$POOL',
    'name':           'zlc_test',
    'encryptiontype': 'aes-256-gcm',
    'key':            '$ZLC_PASS',
    'autounlock':     True,
}))
")
if omv-rpc -u admin "Zfs" "enableEncryption" "$ZLC_ENC_PARAMS" >/dev/null 2>&1; then
    _pass "zlc_test — encrypted dataset created with auto-unlock"
else
    _fail "zlc_test — encrypted dataset creation failed" ""
fi

if [ -f "$ZLC_CACHE" ] && grep -q "^${ZLC_DS}	" "$ZLC_CACHE"; then
    _pass "zfs-list.cache — $ZLC_DS present in cache after creation"
else
    _fail "zfs-list.cache — $ZLC_DS not found in cache after creation" \
        "$(head -5 "$ZLC_CACHE" 2>/dev/null)"
fi

# Reload so zfs-mount-generator picks up the new cache entry and generates
# the zfs-load-key@ unit, giving deleteObject something concrete to stop.
systemctl daemon-reload 2>/dev/null || true

ZLC_INSTANCE=$(systemd-escape -p "$ZLC_DS" 2>/dev/null || echo "")
ZLC_UNIT="zfs-load-key@${ZLC_INSTANCE}.service"

assert_rpc "deleteObject — zlc_test (encrypted with auto-unlock)" \
    "Zfs" "deleteObject" \
    "{\"name\":\"$ZLC_DS\",\"mp\":\"/$ZLC_DS\",\"type\":\"Filesystem\"}"

if [ ! -f "$ZLC_CACHE" ] || ! grep -q "^${ZLC_DS}	" "$ZLC_CACHE"; then
    _pass "zfs-list.cache — $ZLC_DS removed from cache after deleteObject"
else
    _fail "zfs-list.cache — $ZLC_DS still present in cache after deleteObject" ""
fi

if [ -n "$ZLC_INSTANCE" ]; then
    ZLC_SVC_STATE=$(systemctl is-active "$ZLC_UNIT" 2>/dev/null || echo "inactive")
    if [ "$ZLC_SVC_STATE" != "active" ]; then
        _pass "systemd — $ZLC_UNIT not active after deleteObject ($ZLC_SVC_STATE)"
    else
        _fail "systemd — $ZLC_UNIT still active after deleteObject" \
            "expected inactive/dead, got: $ZLC_SVC_STATE"
    fi
else
    info "skipping service state check — systemd-escape produced no output"
fi

zfs destroy -rf "$ZLC_DS" 2>/dev/null || true
unset ZLC_DS ZLC_PASS ZLC_ENC_PARAMS ZLC_CACHE ZLC_INSTANCE ZLC_UNIT ZLC_SVC_STATE

# ===========================================================================
section "Export and import"
# ===========================================================================

# Destroy child filesystems so the pool can be cleanly exported.
info "Removing child datasets before export test"
assert_rpc "deleteObject — fs1/child" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1/child\",\"mp\":\"/$POOL/fs1/child\",\"type\":\"Filesystem\"}"

if ! grep -q "^${POOL}/fs1/child	" "/etc/zfs/zfs-list.cache/$POOL" 2>/dev/null; then
    _pass "zfs-list.cache — fs1/child removed from cache after deleteObject"
else
    _fail "zfs-list.cache — fs1/child still present in cache after deleteObject" ""
fi

# The scheduled snapshot job test may have created snapshots on fs1; remove them.
info "Removing any auto-created snapshots on fs1"
zfs list -H -t snapshot -o name -r "$POOL/fs1" 2>/dev/null | while IFS= read -r snap; do
    zfs destroy "$snap" 2>/dev/null || true
done

assert_rpc "deleteObject — fs1" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs1\",\"mp\":\"/$POOL/fs1\",\"type\":\"Filesystem\"}"

if ! grep -q "^${POOL}/fs1	" "/etc/zfs/zfs-list.cache/$POOL" 2>/dev/null; then
    _pass "zfs-list.cache — fs1 removed from cache after deleteObject"
else
    _fail "zfs-list.cache — fs1 still present in cache after deleteObject" ""
fi

assert_rpc "deleteObject — fs2" "Zfs" "deleteObject" \
    "{\"name\":\"$POOL/fs2\",\"mp\":\"/${POOL}_fs2\",\"type\":\"Filesystem\"}"

if ! grep -q "^${POOL}/fs2	" "/etc/zfs/zfs-list.cache/$POOL" 2>/dev/null; then
    _pass "zfs-list.cache — fs2 removed from cache after deleteObject"
else
    _fail "zfs-list.cache — fs2 still present in cache after deleteObject" ""
fi

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

# Safety net: force-destroy any child datasets that may have survived earlier
# cleanup steps (e.g. a locked encrypted dataset that resisted zfs destroy -r).
info "Force-destroying any remaining child datasets before pool delete"
zfs list -H -o name -r "$POOL" 2>/dev/null | grep -v "^${POOL}$" | sort -r | while IFS= read -r ds; do
    zfs destroy -rf "$ds" 2>/dev/null || true
done

assert_rpc_bg "deleteObjectBg — Pool" "Zfs" "deleteObjectBg" \
    "{\"name\":\"$POOL\",\"mp\":\"/$POOL\",\"type\":\"Pool\"}"

if ! zpool list "$POOL" &>/dev/null; then
    _pass "Pool $POOL destroyed"
else
    _fail "Pool $POOL still exists after deleteObjectBg" ""
fi

if [ ! -f "/etc/zfs/zfs-list.cache/$POOL" ]; then
    _pass "zfs-list.cache — cache file removed after pool deletion"
else
    _fail "zfs-list.cache — cache file still present after pool deletion" ""
fi

# ===========================================================================
section "Orphan cleanup"
# ===========================================================================

# Remove stale zfs-list.cache files left by previous test runs whose pools
# have since been destroyed.
find /etc/zfs/zfs-list.cache/ -type f -name "omvzfs*" -ls -delete 2>/dev/null || true

# Remove stale keyfiles left by previous test runs.
find /etc/zfs/keys/ -name "omvzfstest_*.key" -ls -delete 2>/dev/null || true

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
