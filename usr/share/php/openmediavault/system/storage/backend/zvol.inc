<?php
namespace OMV\System\Storage\Backend;
define("OMV_STORAGE_DEVICE_TYPE_ZVOL", 0x20);
/**
 * Implements the storage device backend for ZFS Zvol devices.
 * @ingroup api
 */
class ZVol extends BackendAbstract
{
    function getType()
    {
        return OMV_STORAGE_DEVICE_TYPE_ZVOL;
    }

    function enumerate()
    {
        return $this->enumerateProcFs("zd[0-9]+");
    }

    function isTypeOf($deviceFile)
    {
        // Examples:
        // - /dev/zd0
        // - /dev/zd1p1
        $regex = "zd[0-9]+(p[0-9]+)?";
        return $this->isTypeOfByName($deviceFile, $regex);
    }

    function getImpl($args)
    {
        return new \OMV\System\Storage\StorageDeviceZvol($args);
    }

    function baseDeviceFile($deviceFile)
    {
        return preg_replace("/(p\d+)$/", "", $deviceFile);
    }

    function fsDeviceFile($deviceFile, $partition = 1)
    {
        // E.g. /dev/zd0p1
        return sprintf("%sp1", $deviceFile);
    }
}
