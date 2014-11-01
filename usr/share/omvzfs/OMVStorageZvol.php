<?php
require_once("openmediavault/system.inc");
require_once("/usr/share/omvzfs/Utils.php");
require_once("/usr/share/omvzfs/Dataset.php");
require_once("/usr/share/omvzfs/Exception.php");

define("OMV_STORAGE_DEVICE_TYPE_ZVOL", 0x20);

/**
 * Implements the storage device backend for ZFS Zvol devices.
 * @ingroup api
 */
class OMVStorageDeviceBackendZvol extends OMVStorageDeviceBackendAbstract {
    function getType() {
        return OMV_STORAGE_DEVICE_TYPE_ZVOL;
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

    function fsDeviceFile($deviceFile) {
        // E.g. /dev/zd0p1
        return sprintf("%sp1", $deviceFile);
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
		$cmd = "/lib/udev/zvol_id \"" . $this->getDeviceFile() . "\"";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$name = $out[0];
        return sprintf(gettext("Zvol %s [%s, %s]"),
          $name, $this->getDeviceFile(), binary_format($this->getSize()));
    }
}

class OMVFilesystemZFS extends OMVFilesystemAbstract {

	public function __construct($fsName) {
		$this->deviceFile = $fsName;
		$this->label = $fsName;
		$this->type = "zfs";
		$this->dataCached = FALSE;
		$this->usage = "filesystem";
	}

	/**
	 * Get the filesystem detailed information.
	 * @private
	 * @return TRUE if successful, otherwise FALSE.
	 */
	private function getData() {
		if ($this->dataCached !== FALSE)
			return TRUE;
		$data = array(
			"devicefile" => $this->deviceFile,
			"label" => $this->label,
			"type" => $this->type,
			"usage" => $this->usage,
		);
		// Set flag to mark information has been successfully read.
		$this->dataCached = TRUE;
		return TRUE;
	}

    /**
     * Refresh the cached information.
     * @return TRUE if successful, otherwise FALSE.
     */
    public function refresh() {
        $this->dataCached = FALSE;
        if ($this->getData() === FALSE)
            return FALSE;
        return TRUE;
    }

	/**
	 * Checks if the filesystem exists.
	 * @return TRUE if the device exists, otherwise FALSE.
	 */
	public function exists() {
		if ($this->getData() === FALSE)
			return FALSE;
		return TRUE;
	}

    /**
     * Check if the filesystem has an UUID.
     * @return FALSE.
     */
    public function hasUuid() {
        return FALSE;
    }

    /**
     * Get the UUID of the filesystem.
	 * @return FALSE.
	 */
	public function getUuid() {
		return FALSE;
	}

    /**
     * Check if the filesystem has a label.
	 * @return Returns TRUE 
	 */
	public function hasLabel() {
		if (FALSE === ($label = $this->getLabel()))
			return FALSE;
		return !empty($label);
	}

    /**
     * Get the filesystem label.
     * @return The filesystem label, otherwise FALSE.
     */
    public function getLabel() {
        if ($this->getData() === FALSE)
            return FALSE;
        return $this->label;
    }

    /**
     * Get the filesystem type, e.g. 'ext3' or 'vfat'.
     * @return The filesystem type, otherwise FALSE.
     */
    public function getType() {
		if ($this->getData() === FALSE)
			return FALSE;
		return $this->type;             
    }

    /**
     * Get the partition scheme, e.g. 'gpt', 'mbr', 'apm' or 'dos'.
     * @deprecated
     * @return The filesystem type, otherwise FALSE.
     */
    public function getPartitionType() {
        return $this->getPartitionScheme();
    }

    /**
     * Get the partition scheme, e.g. 'gpt', 'mbr', 'apm' or 'dos'.
     * @return The filesystem type, otherwise FALSE.
     */
	public function getPartitionScheme() {
		return FALSE;
	}


    /**
     * Get the usage, e.g. 'other' or 'filesystem'.
     * @return The filesystem type, otherwise FALSE.
     */
    public function getUsage() {
        if ($this->getData() === FALSE)
            return FALSE;
        return $this->usage;
    }

    /**
     * Get the partition entry information.
     * @return An array with the fields \em scheme, \em uuid, \em type,
     *   \em flags, \em number, \em offset, \em size and \em disk,
     *   otherwise FALSE.
     */
	public function getPartitionEntry() {
		return FALSE;
	}

    /**
     * Get the device path of the filesystem, e.g /dev/sdb1.
     * @return The device name, otherwise FALSE.
     */
    public function getDeviceFile() {
        if ($this->getData() === FALSE)
            return FALSE;
        return $this->deviceFile;
    }

    /**
     * Get the canonical device file, e.g. /dev/root -> /dev/sde1
     */
    public function getCanonicalDeviceFile() {
        return $this->deviceFile;
    }

    /**
     * Get the device path by UUID, e.g.
     * \li /dev/disk/by-uuid/ad3ee177-777c-4ad3-8353-9562f85c0895
     * \li /dev/disk/by-uuid/2ED43920D438EC29 (NTFS)
     * @return The device path (/dev/disk/by-uuid/xxx) if available,
     *   otherwise /dev/xxx will be returned. In case of an error FALSE
     *   will be returned.
     */
	public function getDeviceFileByUuid() {
		return FALSE;
    }

    /**
     * Get the device file of the storage device containing this
     * file system. Example:
     * <ul>
     * \li /dev/sdb1 => /dev/sdb
     * \li /dev/cciss/c0d0p2 => /dev/cciss/c0d0
     * </ul>
     * @return The device file of the underlying storage device,
     *   otherwise FALSE.
     */
    public function getStorageDeviceFile() {
        if ($this->getData() === FALSE)
            return FALSE;
        $deviceFile = $this->getDeviceFile();
        // The following line is not necessary but will be kept to be safe.
        // If the device file looks like /dev/disk/by-(id|label|path|uuid)/*
        // then it is necessary to get the /dev/xxx equivalent.
        //if (1 == preg_match("/^\/dev\/disk\/by-.+$/", $deviceFile))
        //    $deviceFile = realpath($deviceFile);
        // Truncate the partition appendix, e.g.:
        // - /dev/sdb1 => /dev/sdb
        // - /dev/cciss/c0d0p2 => /dev/cciss/c0d0
        // - /dev/mapper/vg0-lv0 => /dev/mapper/vg0-lv0
        // - /dev/dm-0 => /dev/dm-0
        // - /dev/md0 => /dev/md0
        // - /dev/loop0 => /dev/loop0
        if (NULL === ($backend = OMVStorageDevices::getBackend($deviceFile)))
            return FALSE;
        return $backend->baseDeviceFile($deviceFile);
    }

    /**
     * Get the filesystem block size.
     * @return The block size, otherwise FALSE.
     */
	public function getBlockSize() {
		return FALSE;
		/*
		if ($this->getData() === FALSE)
            return FALSE;
        $cmd = sprintf("export LANG=C; blockdev --getbsz %s",
          escapeshellarg($this->getDeviceFile()));
        @OMVUtil::exec($cmd, $output, $result);
        if ($result !== 0) {
            $this->setLastError($output);
            return FALSE;
        }
		return intval($output[0]);
		 */
    }

    /**
     * Get the mount point of the given filesystem.
     * @return The mountpoint of the filesystem or FALSE.
     */
    public function getMountPoint() {
        if ($this->getData() === FALSE)
			return FALSE;
		$ds = new OMVModuleZFSDataset($this->deviceFile);
		return $ds->getMountPoint();
	}

  	/**
     * Get statistics from a mounted filesystem.
     * @return The filesystem statistics if successful, otherwise FALSE. The
     *   following fields are included: \em devicefile, \em type, \em blocks,
     *   \em size, \em used, \em available, \em percentage and \em mountpoint.
     *   Please note, the fields \em size, \em used and \em available are
     *   strings and their unit is 'B' (bytes).
     */
    public function getStatistics() {
        if ($this->getData() === FALSE)
            return FALSE;
        // Get the mount point of the filesystem.
        if (FALSE === ($mountPoint = $this->getMountPoint()))
            return FALSE;
        OMVModuleZFSUtil::exec("export LANG=C; df -PT", $output, $result);
        if ($result !== 0) {
            $this->setLastError($output);
            return FALSE;
        }
        // Parse command output:
        // Filesystem                                             Type     1024-blocks    Used Available Capacity Mounted on
        // rootfs                                                 rootfs      14801380 1392488  12657020      10% /
        // udev                                                   devtmpfs       10240       0     10240       0% /dev
        // tmpfs                                                  tmpfs         102356     324    102032       1% /run
        // /dev/disk/by-uuid/128917b7-718a-4361-ab6f-6b1725564cb8 ext4        14801380 1392488  12657020      10% /
        // tmpfs                                                  tmpfs           5120       0      5120       0% /run/lock
        // tmpfs                                                  tmpfs         342320       0    342320       0% /run/shm
        // tmpfs                                                  tmpfs         511768       4    511764       1% /tmp
        // /dev/sdb1                                              ext4          100156    4164     95992       5% /media/b994e5e8-94ae-482c-ae54-70a0bbb2737e
        $result = FALSE;
        foreach ($output as $outputk => $outputv) {
            $matches = preg_split("/[\s,]+/", $outputv);
            if (0 !== strcasecmp($mountPoint, $matches[6]))
                continue;
            $result = array(
                "devicefile" => $this->deviceFile,
                "type" => $matches[1],
                "blocks" => $matches[2],
                "size" => bcmul($matches[2], "1024", 0),
                "used" => binary_convert($matches[3], "KiB", "B"),
                "available" => binary_convert($matches[4], "KiB", "B"),
                "percentage" => intval(trim($matches[5], "%")),
                "mountpoint" => $matches[6]
            );
        }
        return $result;
    }

    /**
     * Does the filesystem support POSIX ACL.
     * @return TRUE if the filesystem supports POSIX ACL, otherwise FALSE.
     */
    public function hasPosixAclSupport() {
        if ($this->getData() === FALSE)
            return FALSE;
        if (NULL == ($backend = OMVFilesystems::getBackendByType(
          $this->getType())))
            return FALSE;
        // The list of POSIX ACL supporting filesystems. See:
        // - http://de.wikipedia.org/wiki/Access_Control_List
        // - http://www.suse.de/~agruen/acl/linux-acls/online
        return $backend->hasPosixAclSupport();
    }

    /**
     * Check if a filesystem is mounted.
     * @return TRUE if the filesystem is mounted, otherwise FALSE.
     */
    public function isMounted() {
        if ($this->getData() === FALSE)
			return FALSE;
		$ds = new OMVModuleZFSDataset($this->deviceFile);
		$mounted = $ds->getProperty("mounted");
		if (strcmp($mounted['value'],"yes") === 0) {
			return TRUE;
		} else {
			return FALSE;
		}
	}	

    /**
     * Mount the filesystem by its device file or UUID.
     * @param options Additional mount options. Defaults to "".
     * @return TRUE if successful, otherwise FALSE.
     */
    public function mount($options = "") {
		$cmd = "zfs mount \"" . $this->deviceFile . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return TRUE;
    }

    /**
     * Unmount the filesystem.
     * @param force Set to TRUE to force unmount. Defaults to FALSE.
     * @param lazy Set to TRUE to lazy unmount. Defaults to FALSE.
     * @return TRUE if successful, otherwise FALSE.
     */
    public function umount($force = FALSE, $lazy = FALSE) {
		$cmd = "zfs unmount \"" . $this->deviceFile . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return TRUE;
	}

    /**
     * See if a directory is a mountpoint.
     * @param
     * @return TRUE if the directory is a mountpoint, otherwise FALSE.
     */
    public static function isMountPoint($dir) {
        if (!is_dir($dir))
            return FALSE;
        if (FALSE === ($stat = stat($dir)))
            return FALSE;
        if (FALSE === ($stat2 = stat(sprintf("%s/..", $dir))))
            return FALSE;
        return (($stat.dev != $stat2.dev) || (($stat.dev == $stat2.dev) &&
          ($stat.ino == $stat2.ino))) ? TRUE : FALSE;
    }

    /**
     * Get the directory where the filesystem should be mounted to. Note,
     * this path is OMV specific: /media/<FSUUID>.
     * @param uuid The UUID of the filesystem.
     * @return The path where to mount the filesystem, e.g.
     *   /media/85732966-949a-4d8b-87d7-d7e6681f787e.
     */
    public static function buildMountPath($uuid) {
        return sprintf("%s/%s", $GLOBALS['OMV_MOUNT_DIR'], $uuid);
    }

    /**
     * Check if the given device file contains a file system.
     * @param deviceFile The devicefile to check.
     * @return TRUE if the devicefile has a file system, otherwise FALSE.
     */
    public static function hasFileSystem($deviceFile) {
        // An alternative implementation is:
        // blkid -p -u filesystem <devicefile>
		// Scan output for tag PTTYPE.
		return TRUE;
		/*
        $cmd = sprintf("export LANG=C; blkid | grep -E '^%s[0-9]*:.+$'",
          $deviceFile);
        @OMVUtil::exec($cmd, $output, $result);
        if($result !== 0)
            return FALSE;
		return TRUE;
		*/
    }
}

class OMVFilesystemBackendZFS extends OMVFilesystemBackendAbstract {
    public function __construct() {
        parent::__construct();
        $this->type = "zfs";
        $this->properties = self::PROP_NONE;
    }

    public function getImpl($args) {
        return new OMVFilesystemZFS($args);
    }

	public function isTypeOf($fsName) {
		$cmd = "zfs list -H -o name -t filesystem \"$fsName\"";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		if ($res == 0)
			return TRUE;
		return FALSE;
	}

	public function enumerate() {
		$cmd = "zfs list -H -o name -t filesystem 2>&1";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		$result = array();
		foreach ($out as $name) {
			$data = array("devicefile" => $name,
							"uuid" => "",
							"label" => $name,
							"type" => "zfs");
			$result[$name] = $data;
		}
		return $result;
	}

	public function isBlkidEnumerated() {
		return FALSE;
	}

}

OMVStorageDevices::registerBackend(new OMVStorageDeviceBackendZvol());
OMVFilesystems::registerBackend(new OMVFilesystemBackendZFS());
?>
