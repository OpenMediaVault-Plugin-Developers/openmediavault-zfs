#!/bin/bash
rm -rf openmediavault-zfs/usr/share/openmediavault/engined/rpc
mkdir -p openmediavault-zfs/usr/share/openmediavault/engined/rpc/zfs
rm -rf openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/storage/zfs
mkdir -p openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/storage/zfs
rm -rf openmediavault-zfs/var/www/openmediavault/images
mkdir -p openmediavault-zfs/var/www/openmediavault/images
rm -rf openmediavault-zfs/usr/share/openmediavault/locale
cp src/* openmediavault-zfs/usr/share/openmediavault/engined/rpc/zfs
cp gui/rpc/zfs.inc openmediavault-zfs/usr/share/openmediavault/engined/rpc
cp gui/js/omv/module/admin/storage/zfs/* openmediavault-zfs/var/www/openmediavault/js/omv/module/admin/storage/zfs
cp images/* openmediavault-zfs/var/www/openmediavault/images
cp -r usr/share/openmediavault/locale openmediavault-zfs/usr/share/openmediavault/
cd openmediavault-zfs
dpkg-buildpackage -us -uc

