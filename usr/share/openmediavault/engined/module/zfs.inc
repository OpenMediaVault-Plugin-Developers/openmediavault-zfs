<?php
/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2009-2015 Volker Theile
 * @copyright Copyright (c) 2015-2026 openmediavault plugin developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 class OMVModuleZFS extends \OMV\Engine\Module\ServiceAbstract
    implements \OMV\Engine\Notify\IListener {
    /**
     * Get the module name.
     */
    public function getName()
    {
        return "zfs";
    }

    /**
     * Helper function to find out whether the given shared folder
     * configuration object is used. If it is used, then mark the
     * module as dirty.
     * @param type The event message type.
     * @param path The event message path.
     * @param object The configuration object.
     */
    final public function onSharedFolder($type, $path, $object)
    {
        $db = \OMV\Config\Database::getInstance();
        $share = $db->getAssoc("conf.system.sharedfolder",$object['sharedfolderref']);
        $mntent = $db->getAssoc("conf.system.filesystem.mountpoint",$share['mntentref']);
        $bindmntent = $db->getAssoc("conf.system.filesystem.mountpoint",$object['mntentref']);

        if (!strcmp($mntent['type'],'zfs')) {
            if ((strpos($object['extraoptions'],'nohide') === false) and
              (strpos($object['extraoptions'],'crossmnt') === false)) {
                $object['extraoptions'] = $object['extraoptions'] . ",crossmnt";
                $newshare = new \OMV\Config\ConfigObject("conf.service.nfs.share");
                $newshare->setAssoc($object);
                $db->set($newshare,TRUE);
            }
            if (strpos($bindmntent['opts'], '_netdev') === false) {
                $bindmntent['opts'] = $bindmntent['opts'] . ",_netdev";
                $newbindmntent = new \OMV\Config\ConfigObject("conf.system.filesystem.mountpoint");
                $newbindmntent->setAssoc($bindmntent);
                $db->set($newbindmntent);
                $moduleMngr = \OMV\Engine\Module\Manager::getInstance();
                $fstab = $moduleMngr->getModule("fstab");
                $fstab->setDirty();
            }
        }
    }

    /**
     * Bind listeners.
     */
    function bindListeners(\OMV\Engine\Notify\Dispatcher $dispatcher)
    {
        $dispatcher->addListener(
          OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY,
          "org.openmediavault.conf.service.nfs.share",
          [ $this, "onSharedFolder" ]);
    }
}
