# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2026 openmediavault plugin developers
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

{# --- arch detection --- #}
{% set arch = grains.get('osarch', '') %}
{% set is_amd64 = arch in ['amd64', 'x86_64'] %}
{% set is_arm64 = arch in ['arm64', 'aarch64'] %}

{# --- running kernel flavor --- #}
{% set krel = grains.get('kernelrelease', '') %}
{% set running_pve = ('pve' in krel) %}

{# --- installed Proxmox kernel packages (no shell) --- #}
{% set pkgs = salt['pkg.list_pkgs']() %}
{% set ns = namespace(pve_installed=false) %}
{% for p in pkgs.keys() %}
  {% if p.startswith('pve-kernel-') %}
    {% set ns.pve_installed = true %}
  {% endif %}
{% endfor %}
{% set pve_installed = ns.pve_installed %}

{% set provider_pve  = 'omv-zfs-provider-pve' %}
{% set provider_dkms = 'omv-zfs-provider-dkms' %}
{% set pin_file = '/etc/apt/preferences.d/omv-zfs-pve.pref' %}

{# Select provider based on RUNNING kernel (safe when both kernels installed) #}
{% set use_pve = (is_amd64 and running_pve) %}

{% if use_pve %}
  {% set provider_wanted = provider_pve %}
  {% set provider_remove = provider_dkms %}
{% else %}
  {% set provider_wanted = provider_dkms %}
  {% set provider_remove = provider_pve %}
{% endif %}

zfs_provider_install_selected:
  pkg.installed:
    - name: {{ provider_wanted }}

zfs_provider_remove_other:
  pkg.removed:
    - name: {{ provider_remove }}
    - purge: True
    - require:
      - pkg: zfs_provider_install_selected

{% if use_pve %}
zfs_provider_purge_dkms_bits:
  pkg.removed:
    - pkgs:
      - zfs-dkms
      - dkms
    - purge: True
    - require:
      - pkg: zfs_provider_install_selected

zfs_provider_pve_apt_pin:
  file.managed:
    - name: {{ pin_file }}
    - mode: '0644'
    - user: root
    - group: root
    - contents: |
        # Managed by OMV ZFS plugin
        Package: zfsutils-linux zfs-zed libnvpair* libuutil* libzfs* libzpool* spl* zfs*
        Pin: origin "download.proxmox.com"
        Pin-Priority: 700

        Package: proxmox-headers-*
        Pin: origin "download.proxmox.com"
        Pin-Priority: 700
    - require:
      - pkg: zfs_provider_install_selected

{# Optional: verify zfs module exists for running pve kernel #}
zfs_provider_verify_pve_module:
  cmd.run:
    - name: "modinfo -k $(uname -r) zfs >/dev/null"
    - failhard: True
    - require:
      - pkg: zfs_provider_install_selected

{% else %}
zfs_provider_remove_apt_pin:
  file.absent:
    - name: {{ pin_file }}

{% endif %}

{# Informational notice: pve kernel installed but not booted #}
{% if is_amd64 and pve_installed and not running_pve %}
zfs_provider_pve_not_active_notice:
  test.nop:
    - name: |
        Proxmox kernel packages are installed, but the system is running a non-Proxmox kernel:
          running kernel: {{ krel }}
        ZFS is currently configured for the DKMS provider.
        Reboot into the Proxmox (pve) kernel, then click Apply to switch to Proxmox ZFS modules.
{% endif %}

{# arm64 header sanity check for DKMS path #}
{% if is_arm64 and not use_pve %}
{% set headers_present = (salt['cmd.retcode']("test -e /lib/modules/$(uname -r)/build", python_shell=True) == 0) %}
{% if not headers_present %}
zfs_provider_arm64_missing_headers:
  test.fail_without_changes:
    - name: |
        ZFS DKMS selected on arm64, but kernel headers for the running kernel are missing.
        Running kernel: {{ krel }}
        Expected path: /lib/modules/$(uname -r)/build

        Install the matching Armbian headers for this kernel, then click Apply again.
{% endif %}
{% endif %}
