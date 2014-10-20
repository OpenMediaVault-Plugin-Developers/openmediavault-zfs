<?php
require_once("openmediavault/system.inc");

/**
 * Implements the storage device backend for ZFS Zvol devices.
 * @ingroup api
 */
class OMVStorageDeviceBackendZvol extends OMVStorageDeviceBackendAbstract {
    function getType() {
        return OMV_STORAGE_DEVICE_TYPE_HBA;
    }

    function enumerate() {
        return $this->enumerateProcFs("zd[0-9]+");
    }

    function isTypeOf($deviceFile) {
        // Examples:
        // - /dev/zd0
        // - /dev/zd1p1
        $regex = "zd[0-9]+(p[0-9]+)?";
        return $this->isTypeOfByName($deviceFile, $regex);
    }

    function getImpl($args) {
        return new OMVStorageDeviceZvol($args);
    }

    function baseDeviceFile($deviceFile) {
        return preg_replace("/(p\d+)$/", "", $deviceFile);
    }
}

/**
 * This class provides a simple interface to handle ZFS Zvol devices.
 * @ingroup api
 */
class OMVStorageDeviceZvol extends OMVStorageDeviceAbstract {
    /**
     * Get the description of the device.
     * @return The device description, FALSE on failure.
     */
    public function getDescription() {
        return sprintf(gettext("ZFS Zvol [%s, %s]"),
          $this->getDeviceFile(), binary_format($this->getSize()));
    }
}

OMVStorageDevices::registerBackend(new OMVStorageDeviceBackendZvol());

?>

