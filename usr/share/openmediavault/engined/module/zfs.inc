<?php

/**
 * Copyright (C) 2009-2013 Volker Theile <volker.theile@openmediavault.org>
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
 * Copyright (C) 2015      Ilya Kogan <ikogan@flarecode.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

require_once("openmediavault/module.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/error.inc");

class OMVModuleZfs extends OMVModuleServiceAbstract
	implements OMVINotifyListener {

        /**
         * Get the module name.
         */
        public function getName() {
            return "zfs";
        }

        /**
         * Bind listeners.
         */
        function bindListeners(OMVNotifyDispatcher $dispatcher) {
            $dispatcher->addListener(
              OMV_NOTIFY_MODIFY,
              "org.openmediavault.storage.zfs.filesystem",
              array($moduleMgr->getModule("nfs"), "setDirty"));
            $dispatcher->addListener(
              OMV_NOTIFY_MODIFY,
              "org.openmediavault.storage.zfs.filesystem",
              array($moduleMgr->getModule("fstab"), "setDirty"));
        }
}

// Register module.
$moduleMgr = &OMVModuleMgr::getInstance();
$moduleMgr->registerModule(new OMVModuleZfs());
?>
