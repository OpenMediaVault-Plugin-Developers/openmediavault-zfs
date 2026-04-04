#!/bin/sh
# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2015-2026 openmediavault plugin developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

set -e

. /etc/default/openmediavault
. /usr/share/openmediavault/scripts/helper-functions

if ! omv_config_exists "/config/services/zfs"; then
    omv_config_add_node "/config/services" "zfs"
fi

if ! omv_config_exists "/config/services/zfs/snapshotjobs"; then
    omv_config_add_node "/config/services/zfs" "snapshotjobs"
fi

if ! omv_config_exists "/config/services/zfs/scrubjobs"; then
    omv_config_add_node "/config/services/zfs" "scrubjobs"
fi

if ! omv_config_exists "/config/services/zfs/replicationjobs"; then
    omv_config_add_node "/config/services/zfs" "replicationjobs"
fi

# add zfs-zed notfication
xpath="/config/system/notification/notifications"
if ! omv_config_exists "${xpath}/notification[id='zfs']"; then
  object="<uuid>$(omv_uuid)</uuid>"
  object="${object}<id>zfs</id>"
  object="${object}<enable>0</enable>"
  omv_config_add_node_data "${xpath}" "notification" "${object}"
fi

# Import scrub jobs from zfsutils-linux into the plugin so they can be
# managed from the UI.  Only runs when the scrub line is not yet commented
# out and the ZFS kernel module is loaded.
ZFSUTILS_CRON="/etc/cron.d/zfsutils-linux"
if [ -f "${ZFSUTILS_CRON}" ] && grep -q "^[^#].*zfs-linux/scrub" "${ZFSUTILS_CRON}"; then
  if command -v zpool >/dev/null 2>&1 && zpool list >/dev/null 2>&1; then
    zpool list -H -o name,health 2>/dev/null | while IFS="$(printf '\t')" read -r pool health; do
      [ "${health}" = "ONLINE" ] || continue
      if ! omv_config_exists "/config/services/zfs/scrubjobs/job[pool='${pool}']"; then
        object="<uuid>$(omv_uuid)</uuid>"
        object="${object}<enable>1</enable>"
        object="${object}<pool>${pool}</pool>"
        object="${object}<execution>exactly</execution>"
        object="${object}<minute>24</minute>"
        object="${object}<hour>0</hour>"
        object="${object}<dayofmonth>1</dayofmonth>"
        object="${object}<month>*</month>"
        object="${object}<dayofweek>*</dayofweek>"
        object="${object}<everynminute>0</everynminute>"
        object="${object}<everynhour>0</everynhour>"
        object="${object}<everyndayofmonth>0</everyndayofmonth>"
        object="${object}<sendemail>0</sendemail>"
        object="${object}<emailonerror>0</emailonerror>"
        object="${object}<comment>Imported from zfsutils-linux</comment>"
        omv_config_add_node_data "/config/services/zfs/scrubjobs" "job" "${object}"
      fi
    done
    # Disable the system scrub line to avoid double-scrubbing; the TRIM
    # line in the same file is intentionally left untouched.
    sed -i 's|^\([^#].*zfs-linux/scrub.*\)|# \1|' "${ZFSUTILS_CRON}" || true
  fi
fi

exit 0
