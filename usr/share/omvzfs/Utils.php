<?php
require_once("Exception.php");
require_once("openmediavault/util.inc");
require_once("Dataset.php");
require_once("Zvol.php");
require_once("Vdev.php");
require_once("Zpool.php");

/**
 * Helper class for ZFS module
 */
class OMVModuleZFSUtil {

	/**
	 * Returns the latest time a pool was scrubbed.
	 *
	 */
	public static function latestScrub($name) {
		$cmd = "zpool status \"" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		foreach ($out as $line) {
			if (preg_match('/none requested/', $line)) 
				return "Never";
			if (preg_match('/with [\d]+ errors on (.*)/', $line, $matches))
				return $matches[1];
			if (preg_match('/scrub (in progress since .*)/', $line, $matches))
				return $matches[1];
		}
	}

	/**
	 * Gets the Zvol device name (zdX) from volume name
	 * 
	 */
	public static function getZvolDev($name) {
		$cmd = "ls -la \"/dev/zvol/" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		preg_match('/(zd[0-9]+)$/', $out[0], $match);
		return($match[1]);
	}

	/**
	 * Sets a GPT label on a disk to prevent the zpool command from generating
	 * errors.
	 *
	 */
	public static function setGPTLabel($disk) {
		$cmd = "parted -s " . $disk . " mklabel gpt 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
	}

	/**
	 * Manages relocation of ZFS filesystem mountpoints in the OMV backend.
	 * Needed when the user changes mountpoint of a filesystem in the GUI.
	 *
	 */
	public static function relocateFilesystem($name) {
		global $xmlConfig;
		$poolname = OMVModuleZFSUtil::getPoolname($name);
		$pooluuid = OMVModuleZFSUtil::getUUIDbyName($poolname);
		$ds = new OMVModuleZFSDataset($name);
		$dir = $ds->getMountPoint();
		$xpath = "//system/fstab/mntent[fsname='" . $pooluuid . "' and dir='" . $dir . "' and type='zfs']";
		$object = $xmlConfig->get($xpath);
		$object['dir'] = $property['value'];
		$xmlConfig->replace($xpath, $object);
		return null;
	}
	
	/**
	 * Clears all ZFS labels on specified devices.
	 * Needed for blkid to display proper data.
	 *
	 */
	public static function clearZFSLabel($disks) {
		foreach ($disks as $disk) {
			$cmd = "zpool labelclear " . $disk . " 2>&1";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
		}
		return null;
	}

	/**
	 * Return all disks in /dev/sdXX used by the pool
	 *
	 * @return array An array with all the disks
	 */
	public static function getDevDisksByPool($name) {
		$pool = new OMVModuleZFSZpool($name);
		$disks = array();
		$vdevs = $pool->getVdevs();
		foreach ($vdevs as $vdev) {
			$vdisks = $vdev->getDisks();
			foreach ($vdisks as $vdisk) {
				if (preg_match('/^sd[a-z]{1}$/', $vdisk)) {
					$disks[] = "/dev/" . $vdisk . "1";
					continue;
				} else if (preg_match('/^pci[a-z0-9-:.]+$/', $vdisk)) {
					$disks[] = "/dev/" . OMVModuleZFSUtil::getDevByPath($vdisk) . "1";
					continue;
				} else if (!(OMVModuleZFSUtil::getDevByID($vdisk) === null)) {
					$disks[] = "/dev/" . OMVModuleZFSUtil::getDevByID($vdisk) . "1";
					continue;
				} else if (preg_match('/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/', $vdisk)) {
					$disks[] = getDevByUuid($vdisk);
					continue;
				} else {
					throw new OMVModuleZFSException("Unknown disk identifier " . $vdisk);
				}
			}
		}
		return($disks);
	}
	
	/**
	 * Get the /dev/sdX device name from /dev/disk/by-uuid
	 *
	 */
	public static function getDevByUuid($uuid) {
		$cmd = "ls -la /dev/disk/by-uuid/" . $uuid;
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		if (count($out) === 1) {
			if (preg_match('/^.*\/([a-z0-9]+)$/', $out[0], $match)) {
				$disk = $match[1];
				return($disk);
			}
		}
		throw new OMVModuleZFSException("Unable to find /dev/disk/by-uuid/" . $uuid);
	}

	/**
	 * Get the /dev/sdX device name from /dev/disk/by-id
	 *
	 */
	public static function getDevByID($id) {
		$cmd = "ls -la /dev/disk/by-id/" . $id;
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		if (count($out) === 1) {
			if (preg_match('/^.*\/([a-z0-9]+)$/', $out[0], $match)) {
				$disk = $match[1];
				return($disk);
			}
		}
		return(null);
	}

	/**
	 * Get the /dev/sdX device name from /dev/disk/by-path
	 *
	 */
	public static function getDevByPath($path) {
		$cmd = "ls -la /dev/disk/by-path/" . $path;
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		if (count($out) === 1) {
			if (preg_match('/^.*\/([a-z0-9]+)$/', $out[0], $match)) {
				$disk = $match[1];
				return($disk);
			}
		}
		throw new OMVModuleZFSException("Unable to find /dev/disk/by-path/" . $path);
	}

	/**
	 * Deletes all shared folders pointing to the specifc path
	 *
	 */
	public static function deleteShares($name) {
		global $xmlConfig;
		$poolname = OMVModuleZFSUtil::getPoolname($name);
		$pooluuid = OMVModuleZFSUtil::getUUIDbyName($poolname);
		$ds = new OMVModuleZFSDataset($name);
		$dir = $ds->getMountPoint();
		$xpath = "//system/fstab/mntent[fsname='" . $pooluuid . "' and dir='" . $dir . "' and type='zfs']";
		$mountpoint = $xmlConfig->get($xpath);
		$mntentuuid = $mountpoint['uuid'];
		$xpath = "//system/shares/sharedfolder[mntentref='" . $mntentuuid . "']";
		$objects = $xmlConfig->getList($xpath);
		foreach ($objects as $object) {
			$tmpxpath = sprintf("//*[contains(name(),'sharedfolderref')]".
				"[contains(.,'%s')]", $object['uuid']);
			if ($xmlConfig->exists($tmpxpath)) {
				throw new OMVModuleZFSException("The Filesystem is shared and in use. Please delete all references and try again.");
			}
		}
		$xmlConfig->delete($xpath);
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_DELETE,"org.openmediavault.system.shares.sharedfolder",$object);
	}

	/**
	 * Get the relative path by complete path
	 *
	 * @return string Relative path of the complet path
	 */
	public static function getReldirpath($path) {
 		$subdirs = preg_split('/\//',$path);
		$reldirpath = "";
		for ($i=2;$i<count($subdirs);$i++) {
			$reldirpath .= $subdirs[$i] . "/";
		}
		return(rtrim($reldirpath, "/"));

	}

	/**
	 * Get /dev/disk/by-path from /dev/sdX
	 *
	 * @return string Disk identifier
	 */
	public static function getDiskPath($disk) {
		preg_match("/^.*\/([A-Za-z0-9]+)$/", $disk, $identifier);
		$cmd = "ls -la /dev/disk/by-path | grep '$identifier[1]$'";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		if (is_array($out)) {
			$cols = preg_split('/[\s]+/', $out[0]);
			return($cols[count($cols)-3]);
		}
	}


	/**
	 * Get poolname from name of dataset/volume etc.
	 *
	 * @return string Name of the pool
	 */
	public static function getPoolname($name) {
		$tmp = preg_split('/[\/]+/', $name);
		return($tmp[0]);
	}

	/**
	 * Get UUID of ZFS pool by name
	 *
	 * @return string UUID of the pool
	 */
	public static function getUUIDbyName($poolname) {
		$cmd = "zpool get guid \"" . $poolname . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		if (isset($out)) {
			$headers = preg_split('/[\s]+/', $out[0]);
			for ($i=0; $i<count($headers); $i++) {
				if (strcmp($headers[$i], "VALUE") === 0) {
					$valuecol=$i;
					break;
				}
			}
			$line = preg_split('/[\s]+/', $out[1]);
			return $line[$valuecol];
		}
		return null;
	}

	/**
	 * Add any missing ZFS filesystems to the OMV backend
	 *
	 */
	public static function addMissingOMVMntEnt() {
		global $xmlConfig;
		$cmd = "zfs list -H -o name -t filesystem";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		foreach($out as $name) {
			$ds = new OMVModuleZFSDataset($name);
			$dir = $ds->getMountPoint();
			$xpath = "//system/fstab/mntent[fsname='" . $name . "' and dir='" . $dir . "' and type='zfs']";
			if (!($xmlConfig->exists($xpath))) {
				$uuid = OMVUtil::uuid();
				$object = array(
					"uuid" => $uuid,
					"fsname" => $name,
					"dir" => $dir,
					"type" => "zfs",
					"opts" => "rw,relatime,xattr,noacl",
					"freq" => "0",
					"passno" => "0",
					"hidden" => "1"
				);
				$xmlConfig->set("//system/fstab",array("mntent" => $object));
			}
		}
		return null;
	}

	/**
	 * Get an array with all ZFS objects
	 *
	 * @return An array with all ZFS objects
	 */
	public static function getZFSFlatArray() {
		$prefix = "root/pool-";
		$objects = array();
		$cmd = "zfs list -H -t all -o name,type 2>&1";
		$expanded = true;
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		foreach ($out as $line) {
			$parts = preg_split('/\t/',$line);
			$path = $parts[0];
			$type = $parts[1];
			$subdirs = preg_split('/\//',$path);
			$root = $subdirs[0];
			$tmp = array();

			switch ($type) {
			case "filesystem":
				if (strpos($path,'/') === false) {
					//This is a Pool
					$tmp = array('id'=>$prefix . $path,
						'parentid'=>'root',
						'name'=>$path,
						'type'=>'Pool',
						'icon'=>'images/raid.png',
						'expanded'=>$expanded,
						'path'=>$path);
					$pool = new OMVModuleZFSZpool($path);
					$tmp['origin'] = "n/a";
					$tmp['size'] = $pool->getSize();
					$tmp['used'] = $pool->getAttribute("allocated");
					$tmp['available'] = $pool->getAttribute("free");
					$tmp['mountpoint'] = $pool->getMountPoint();
					$tmp['lastscrub'] = OMVModuleZFSUtil::latestScrub($path);
					array_push($objects,$tmp);
				} else {
					//This is a Filesystem
					preg_match('/(.*)\/(.*)$/', $path, $result);
					$tmp = array('id'=>$prefix . $path,
						'parentid'=>$prefix . $result[1],
						'name'=>$result[2],
						'icon'=>"images/filesystem.png",
						'path'=>$path,
						'expanded'=>$expanded);
					$ds =  new OMVModuleZFSDataset($path);
					if ($ds->isClone()) {
						//This is a cloned Filesystem
						$tmp['origin'] = $ds->getOrigin();
					} else {
						//This is a standard Filesystem.
						$tmp['origin'] = "n/a";
					}
					$tmp['type']= ucfirst($type);
					$tmp['size'] = "n/a";
					$used = $ds->getProperty("used");
					$tmp['used'] = $used['value'];
					$available = $ds->getProperty("available");
					$tmp['available'] = $available['value'];
					$tmp['mountpoint'] = $ds->getMountPoint();
					$tmp['lastscrub'] = "n/a";
					array_push($objects,$tmp);
				}
				break;

			case "volume":
				preg_match('/(.*)\/(.*)$/', $path, $result);
				$tmp = array('id'=>$prefix . $path,
					'parentid'=>$prefix . $result[1],
					'name'=>$result[2],
					'type'=>ucfirst($type),
					'icon'=>"images/save.png",
					'path'=>$path,
					'expanded'=>$expanded);
				$vol = new OMVModuleZFSZvol($path);
				if ($vol->isClone()) {
					//This is a cloned Volume
					$tmp['origin'] = $vol->getOrigin();
				} else {
					//This is a standard Volume
					$tmp['origin'] = "n/a";
				}
				$tmp['type']= ucfirst($type);
				$tmp['size'] = $vol->getSize();
				$tmp['used'] = $vol->getUsed();
				$tmp['available'] = $vol->getAvailable();
				$tmp['mountpoint'] = "n/a";
				$tmp['lastscrub'] = "n/a";
				array_push($objects,$tmp);
				break;

			case "snapshot":
				preg_match('/(.*)\@(.*)$/', $path, $result);
				$subdirs = preg_split('/\//',$result[1]);
				$root = $subdirs[0];
				$tmp = array('id'=>$prefix . $path,
					'parentid'=>$prefix . $result[1],
					'name'=>$result[2],
					'type'=>ucfirst($type),
					'icon'=>'images/zfs_snap.png',
					'path'=>$path,
					'expanded'=>$expanded);
				$tmp['origin'] = "n/a";
				$tmp['size'] = "n/a";
				$tmp['used'] = "n/a";
				$tmp['available'] = "n/a";
				$tmp['mountpoint'] = "n/a";
				$tmp['lastscrub'] = "n/a";
				array_push($objects,$tmp);
				break;

			default:
				break;
			}
		}
		return $objects;
	}

	/**
	 * Create a tree structured array
	 *
	 * @param &$list The flat array to convert to a tree structure
	 * @param $parent Root node of the tree to create
	 * @return Tree structured array
	 *
	 */
	public static function createTree(&$list, $parent){
		$tree = array();
		foreach ($parent as $k=>$l){
			if(isset($list[$l['id']])){
				$l['leaf'] = false;
				$l['children'] = OMVModuleZFSUtil::createTree($list, $list[$l['id']]);
			} else {
				$l['leaf'] = true;
			}
			$tree[] = $l;
		}
		return $tree;
	}

	/**
	 * Get all Datasets as objects
	 * 
	 * @return An array with all the Datasets
	 */
	public static function getAllDatasets() {
		$datasets = array();
		$cmd = "zfs list -H -t filesystem -o name 2>&1";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		foreach ($out as $name) {
			$ds = new OMVModuleZFSDataset($name);
			array_push($datasets, $ds);
		}
		return $datasets;
	}

	/**
	 * Helper function to execute a command and throw an exception on error
	 * (requires stderr redirected to stdout for proper exception message).
	 * 
	 * @param string $cmd Command to execute
	 * @param array &$out If provided will contain output in an array
	 * @param int &$res If provided will contain Exit status of the command
	 * @return string Last line of output when executing the command
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public static function exec($cmd, &$out = null, &$res = null) {
		$tmp = OMVUtil::exec($cmd, $out, $res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		return $tmp;
	}

	/**
	 * Convert bytes to human readable format
	 *
	 * @param integer bytes Size in bytes to convert
	 * @return string
	 */
	public static function bytesToSize($bytes, $precision = 2) {	
		$kilobyte = 1024;
		$megabyte = $kilobyte * 1024;
		$gigabyte = $megabyte * 1024;
		$terabyte = $gigabyte * 1024;
		
		if (($bytes >= 0) && ($bytes < $kilobyte)) {
			return $bytes . 'B';
	
		} elseif (($bytes >= $kilobyte) && ($bytes < $megabyte)) {
			return round($bytes / $kilobyte, $precision) . 'K';
	
		} elseif (($bytes >= $megabyte) && ($bytes < $gigabyte)) {
			return round($bytes / $megabyte, $precision) . 'MB';
	
		} elseif (($bytes >= $gigabyte) && ($bytes < $terabyte)) {
			return round($bytes / $gigabyte, $precision) . 'GB';
	
		} elseif ($bytes >= $terabyte) {
			return round($bytes / $terabyte, $precision) . 'TB';
		} else {
			return $bytes . 'B';
		}
	}

}


?>
