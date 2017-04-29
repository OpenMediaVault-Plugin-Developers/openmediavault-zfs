<?php
namespace OMV\System\Storage;
require_once("/usr/share/omvzfs/Utils.php");

/**
 * This class provides a simple interface to handle ZFS Zvol devices.
 * @ingroup api
 */
class StorageDeviceZvol extends StorageDevice
{
    /**
     * Get the description of the device.
     * @return The device description, FALSE on failure.
     */
    public function getDescription()
    {
        $cmd = "/lib/udev/zvol_id \"" . $this->getDeviceFile() . "\"";
        \OMVModuleZFSUtil::exec($cmd,$out,$res);
        $name = $out[0];
        return sprintf(gettext("Zvol %s [%s, %s]"),
          $name, $this->getDeviceFile(), binary_format($this->getSize()));
    }
}