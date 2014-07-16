#!/bin/bash
rm -rf openmediavault-zfs/usr/share/openmediavault/engined/rpc
mkdir -p openmediavault-zfs/usr/share/openmediavault/engined/rpc/zfs
rm -rf openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/service/zfs
mkdir -p openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/service/zfs
rm -rf openmediavault-zfs/var/www/openmediavault/images
mkdir -p openmediavault-zfs/var/www/openmediavault/images
cp src/* openmediavault-zfs/usr/share/openmediavault/engined/rpc/zfs
cp gui/rpc/zfs.inc openmediavault-zfs/usr/share/openmediavault/engined/rpc
cp gui/js/omv/module/admin/service/zfs/* openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/service/zfs
cp images/* openmediavault-zfs/var/www/openmediavault/images
cd openmediavault-zfs
dpkg-buildpackage -us -uc

