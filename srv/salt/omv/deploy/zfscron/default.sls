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

{% set snapshot_jobs = salt['omv_conf.get_by_filter'](
  'conf.service.zfs.snapshotjob',
  {'operator': 'stringNotEquals', 'arg0': 'uuid', 'arg1': ''}) | default([]) %}

{% set scrub_jobs = salt['omv_conf.get_by_filter'](
  'conf.service.zfs.scrubjob',
  {'operator': 'stringNotEquals', 'arg0': 'uuid', 'arg1': ''}) | default([]) %}

{% set replication_jobs = salt['omv_conf.get_by_filter'](
  'conf.service.zfs.replicationjob',
  {'operator': 'stringNotEquals', 'arg0': 'uuid', 'arg1': ''}) | default([]) %}

configure_zfs_snapshot_cron:
  file.managed:
    - name: "/etc/cron.d/openmediavault-zfs-snapshots"
    - source:
      - salt://{{ tpldir }}/files/etc_cron.d_omv-zfs-snapshots.j2
    - template: jinja
    - context:
        jobs: {{ snapshot_jobs | json }}
    - user: root
    - group: root
    - mode: 644

configure_zfs_scrub_cron:
  file.managed:
    - name: "/etc/cron.d/openmediavault-zfs-scrub"
    - source:
      - salt://{{ tpldir }}/files/etc_cron.d_omv-zfs-scrub.j2
    - template: jinja
    - context:
        jobs: {{ scrub_jobs | json }}
    - user: root
    - group: root
    - mode: 644

configure_zfs_replication_cron:
  file.managed:
    - name: "/etc/cron.d/openmediavault-zfs-replication"
    - source:
      - salt://{{ tpldir }}/files/etc_cron.d_omv-zfs-replication.j2
    - template: jinja
    - context:
        jobs: {{ replication_jobs | json }}
    - user: root
    - group: root
    - mode: 644
