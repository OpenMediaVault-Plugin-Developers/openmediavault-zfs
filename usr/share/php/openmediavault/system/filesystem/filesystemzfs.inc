<?php
namespace OMV\System\Filesystem;
require_once("/usr/share/omvzfs/Filesystem.php");

class FilesystemZfs implements FilesystemInterface,
	SharedFolderCandidateInterface
{
    public function __construct($fsName)
    {
        $this->deviceFile = $fsName;
        $this->label = $fsName;
        $this->type = "zfs";
        $this->dataCached = FALSE;
        $this->usage = "filesystem";
        $this->filesystem = new \OMVModuleZFSFilesystem($this->deviceFile);
    }

    /**
     * Get the partition scheme, e.g. 'gpt', 'mbr', 'apm' or 'dos'.
     * @deprecated
     * @return The filesystem type, otherwise NULL.
     */
    public function getPartitionType()
    {
        return $this->getPartitionScheme();
    }

    /**
     * Get the partition scheme, e.g. 'gpt', 'mbr', 'apm' or 'dos'.
     * @return The filesystem type, otherwise NULL.
     */
    public function getPartitionScheme()
    {
        return NULL;
    }

    /**
     * Get the usage, e.g. 'other' or 'filesystem'.
     * @return The filesystem type, otherwise FALSE.
     */
    public function getUsage()
    {
        return $this->usage;
    }

    /**
     * Get the partition entry information.
     * @return An array with the fields \em scheme, \em uuid, \em type,
     *   \em flags, \em number, \em offset, \em size and \em disk,
     *   otherwise NULL.
     */
    public function getPartitionEntry()
    {
        return NULL;
    }

    /**
     * Get the mount point of the given filesystem.
     * @return The mountpoint of the filesystem or FALSE.
     */
    public function getMountPoint()
    {
        $this->filesystem->updateProperty("mountpoint");
        return $this->filesystem->getMountPoint();
    }

     /**
     * Get statistics from a mounted filesystem.
     * @return The filesystem statistics if successful, otherwise FALSE. The
     *   following fields are included: \em devicefile, \em type, \em blocks,
     *   \em size, \em used, \em available, \em percentage and \em mountpoint.
     *   Please note, the fields \em size, \em used and \em available are
     *   strings and their unit is 'B' (bytes).
     */
    public function getStatistics()
    {
        $this->filesystem->updateAllProperties();
        $result = [
            "devicefile" => $this->deviceFile,
            "type" => "zfs",
            "blocks" => $this->filesystem->getSize() / 1024,
            "size" => $this->filesystem->getSize(),
            "used" => $this->filesystem->getUsed(),
            "available" => $this->filesystem->getAvailable(),
            "percentage" => intval($this->filesystem->getUsed() / $this->filesystem->getSize() * 100),
            "mountpoint" => $this->filesystem->getMountPoint()
        ];
        return $result;
    }

    /**
     * Checks if the filesystem exists.
     * @return TRUE if the device exists, otherwise FALSE.
     */
    public function exists()
    {
        return $this->filesystem->exists();
    }

    /**
     * Get the device path of the filesystem, e.g /dev/sdb1.
     * @return The device name, otherwise FALSE.
     */
    public function getDeviceFile()
    {
        return $this->deviceFile;
    }

    public function getDeviceName()
    {
        return $this->deviceFile;
    }

    public function getDetails()
    {
        return '';
    }

    /**
     * Get the canonical device file, e.g. /dev/root -> /dev/sde1
     */
    public function getCanonicalDeviceFile()
    {
        return $this->deviceFile;
    }

    /**
     * Get the device file by ID, e.g. <ul>
     * \li /dev/disk/by-id/wwn-0x5000cca211cc703c
     * \li /dev/disk/by-id/scsi-SATA_IBM-DHEA-36481_SG0SGF08038
     * \li /dev/disk/by-id/ata-Hitachi_HDT725032VLA360_VFD200R2CWB7ML-part2
     * </ul>
     * The following order of paths will be retured if available: <ul>
     * \li ata-xxx
     * \li wwn-xxx
     * \li scsi-xxx
     * \li ...
     * </ul>
     * @return The device file (/dev/disk/by-id/xxx) if available,
     *   otherwise NULL will be returned.
     */
    public function getDeviceFileById()
    {
		return NULL;
    }

	/**
	 * Check whether the device has a /dev/disk/by-id/xxx device path.
	 * @return Returns TRUE if a disk/by-id device path exists,
	 *   otherwise FALSE.
	 */
	public function hasDeviceFileById() {
		return FALSE;
	}

    /**
     * Get the device path by UUID, e.g.
     * \li /dev/disk/by-uuid/ad3ee177-777c-4ad3-8353-9562f85c0895
     * \li /dev/disk/by-uuid/2ED43920D438EC29 (NTFS)
	 * @return The device file (/dev/disk/by-uuid/xxx) if available,
	 *   otherwise NULL will be returned.
     */
    public function getDeviceFileByUuid()
    {
        return NULL;
    }

	/**
	 * Check whether the filesystem has a /dev/disk/by-uuid/xxx device path.
	 * @return Returns TRUE if a disk/by-uuid device path exists,
	 *   otherwise FALSE.
	 */
	public function hasDeviceFileByUuid() {
		return FALSE;
	}

    /**
     * Get the device path by label, e.g. <ul>
     * \li /dev/disk/by-label/data
     * </ul>
	 * @return The device file (/dev/disk/by-label/xxx) if available,
	 *   otherwise NULL will be returned.
     */
    public function getDeviceFileByLabel()
    {
        return NULL;
    }

	/**
	 * Check whether the filesystem has a /dev/disk/by-label/xxx device path.
	 * @return Returns TRUE if a disk/by-label device path exists,
	 *   otherwise FALSE.
	 */
	public function hasDeviceFileByLabel() {
		return FALSE;
	}

    /**
     * Get a predictable device file in the following order:
     * <ul>
     * \li /dev/disk/by-label/xxx
     * \li /dev/disk/by-id/xxx
     * \li /dev/disk/by-path/xxx
     * \li /dev/xxx
     * </ul>
     * @return Returns a device file.
     */
    public function getPredictableDeviceFile()
    {
        return $this->deviceFile;
    }

    /**
     * Get the special device file to present in the UI instead of the
     * value of the getDeviceFile method.
     * Get the special device file in the following preferred order:
     * <ul>
     * \li /dev/disk/by-label/xxx
     * \li /dev/disk/by-uuid/xxx
     * \li /dev/disk/by-id/xxx
     * \li /dev/xxx
     * </ul>
     * @return Returns the special device file.
     */
    public function getPreferredDeviceFile()
    {
        return $this->deviceFile;
    }

    /**
    * Get all device file symlinks via udev, e.g. <ul>
    * \li /dev/disk/by-id/wwn-0x5000cca211cc703c
    * \li /dev/disk/by-id/scsi-SATA_IBM-DHEA-36481_SG0SGF08038
    * \li /dev/disk/by-id/ata-Hitachi_HDT725032VLA360_VFD200R2CWB7ML
    * \li /dev/disk/by-path/pci-0000:00:02.5-scsi-0:0:0:0
    * \li /dev/disk/by-id/ata-WDC_WD15EARS-00MVWB0_WD-WMAZB2574325-part1
    * \li /dev/disk/by-uuid/fc3e1da5-fd8d-4fda-341e-d0135efa7a7c
    * </ul>
    * @return Returns an string array of device files.
    */
    public function getDeviceFileSymlinks()
    {
        return [];
    }

    public function getDeviceFiles()
    {
        return $this->deviceFile;
    }

    /**
     * Get the device file of the storage device which contains this
     * file system.
     * Example:
     * <ul>
     * \li /dev/sdb1 => /dev/sdb
     * \li /dev/cciss/c0d0p2 => /dev/cciss/c0d0
     * </ul>
	 * @return The device file of the underlying storage device or NULL
	 *   on failure.
     */
    public function getParentDeviceFile()
    {
        return NULL;
    }

    public function hasFstabSupport()
    {
        return FALSE;
    }

    public function hasMultipleDevices()
    {
        return FALSE;
    }

    /**
     * Check if the filesystem has an UUID.
     * @return FALSE.
     */
    public function hasUuid()
    {
        return FALSE;
    }

    /**
     * Get the UUID of the filesystem.
     * @return FALSE.
     */
    public function getUuid()
    {
        return NULL;
    }

    /**
     * Check if the filesystem has a label.
     * @return Returns TRUE
     */
    public function hasLabel()
    {
        if (FALSE === ($label = $this->getLabel()))
            return FALSE;
        return !empty($label);
    }

    /**
     * Get the filesystem label.
     * @return The filesystem label, otherwise FALSE.
     */
    public function getLabel()
    {
        return $this->label;
    }

    /**
     * Get the filesystem type, e.g. 'ext3' or 'vfat'.
     * @return The filesystem type, otherwise FALSE.
     */
    public function getType()
    {
        return $this->type;
    }

    /**
     * Get the size of the device in bytes.
     * @return The size of the device in bytes.
     * @throw \OMV\Exception
     */
    public function getSize()
    {
        return 0;
    }

    /**
     * Set the backend of this filesystem.
     */
    final public function setBackend(Backend\BackendAbstract $backend)
    {
        $this->backend = $backend;
    }

    /**
     * Get the backend of this filesystem.
     */
    final public function getBackend()
    {
        return $this->backend;
    }

    /**
     * Get the description (e.g. incl. additional information like used
     * and available disk space usage) of the file system.
     * @return The file system description.
     */
    public function getDescription()
    {
        $statistics = $this->getStatistics();
        return sprintf(
            '%s [%s (%d%%) used, %s available]',
            $this->getDeviceFile(),
            binary_format($statistics['used']),
            $statistics['percentage'],
            binary_format($statistics['available'])
        );
    }

    /**
     * Check if a filesystem is mounted.
     * @return TRUE if the filesystem is mounted, otherwise FALSE.
     */
    public function isMounted()
    {
        $this->filesystem->updateProperty("mounted");
        $mounted=$this->filesystem->getProperty("mounted");
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
    public function mount($options = "")
    {
        try{
            return $this->filesystem->mount();
        }
        catch (\OMV\ExecException $e) {
            return FALSE;
        }
    }

    /**
     * Unmount the filesystem.
     * @param force Set to TRUE to force unmount. Defaults to FALSE.
     * @param lazy Set to TRUE to lazy unmount. Defaults to FALSE.
     * @return TRUE if successful, otherwise FALSE.
     */
    public function umount($force = FALSE, $lazy = FALSE, $directory = FALSE)
    {
        try{
            $this->filesystem->unmount();
            return TRUE;
        }
        catch (\OMV\ExecException $e) {
            return FALSE;
        }
    }

    public function grow()
    {

    }

    public function shrink()
    {

    }

    public function remove()
    {
        try {
            $this->filesystem->destroy();
            return TRUE;
        }
        catch (\OMV\ExecException $e) {
            return FALSE;
        }
    }
}
