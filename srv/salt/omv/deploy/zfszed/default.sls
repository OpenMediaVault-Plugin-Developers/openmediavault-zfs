# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2019-2026 openmediavault plugin developers
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

{% set notification_config = salt['omv_conf.get_by_filter'](
  'conf.system.notification.notification',
  {'operator': 'stringEquals', 'arg0': 'id', 'arg1': 'zfs'})[0] %}
{% set email_config = salt['omv_conf.get']('conf.system.notification.email') %}

create_zed-rc_config:
  file.managed:
    - name: "/etc/zfs/zed.d/zed.rc"
    - source:
      - salt://{{ tpldir }}/files/etc_zfs_zed-d_zed-rc.j2
    - template: jinja
    - context:
        email_config: {{ email_config | json }}
        notification_config: {{ notification_config | json }}
    - user: root
    - group: root
    - mode: 644

restart_zed_service:
  service.running:
    - name: zfs-zed
    - enable: True
    - watch:
      - file: create_zed-rc_config

