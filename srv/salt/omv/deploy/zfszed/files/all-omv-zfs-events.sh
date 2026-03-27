#!/bin/sh
# Append one line per ZFS event to the OMV ZFS events log.
# Invoked by ZED for every pool event.

LOGFILE="/var/log/omv-zfs-events.log"
POOL="${ZEVENT_POOL:--}"

if [ "${ZEVENT_SUBCLASS}" = "history_event" ] && [ -n "${ZEVENT_HISTORY_INTERNAL_STR}" ]; then
    CLASS="history_event: ${ZEVENT_HISTORY_INTERNAL_STR}"
else
    CLASS="${ZEVENT_SUBCLASS:-unknown}"
fi

echo "$(date -Iseconds): ${POOL} ${CLASS}" >> "${LOGFILE}"
exit 0
