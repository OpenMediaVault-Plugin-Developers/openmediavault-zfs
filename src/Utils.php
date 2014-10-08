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
			$cmd = "zpool labelclear /dev/" . $disk . "1";
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
				if (preg_match('/^[a-z0-9]+$/', $vdisk)) {
					$disks[] = $vdisk;
					continue;
				}
				$cmd = "ls -la /dev/disk/by-path/" . $vdisk;
				unset($out);
				OMVModuleZFSUtil::exec($cmd,$out,$res);
				if (count($out) === 1) {
					if (preg_match('/^.*\/([a-z0-9]+)$/', $out[0], $match)) {
						$disks[] = $match[1];
					}
				}
			}
		}
		return($disks);
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
		$cmd = "zpool get guid " . $poolname . " 2>&1";
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
			if (preg_match('/[\/]+/', $name)) {
				$poolname = OMVModuleZFSUtil::getPoolname($name);
				$pooluuid = OMVModuleZFSUtil::getUUIDbyName($poolname);
				$ds = new OMVModuleZFSDataset($name);
				$dir = $ds->getMountPoint();
				$xpath = "//system/fstab/mntent[fsname='" . $pooluuid . "' and dir='" . $dir . "' and type='zfs']";
				if (!($xmlConfig->exists($xpath))) {
					$uuid = OMVUtil::uuid();
					$object = array(
						"uuid" => $uuid,
						"fsname" => $pooluuid,
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
					$tmp['size'] = $pool->getSize();
					$tmp['used'] = $pool->getAttribute("allocated");
					$tmp['available'] = $pool->getAttribute("free");
					$tmp['mountpoint'] = $pool->getMountPoint();
					$vdevs = $pool->getVdevs();
					$vdev_type = $vdevs[0]->getType();
					switch ($vdev_type) {
					case OMVModuleZFSVdevType::OMVMODULEZFSMIRROR:
						$pool_type = "Mirror";
						break;
					case OMVModuleZFSVdevType::OMVMODULEZFSPLAIN:
						$pool_type = "Basic";
						break;
					case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1:
						$pool_type = "Raidz1";
						break;
					case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2:
						$pool_type = "Raidz2";
						break;
					case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3:
						$pool_type = "Raidz3";
						break;
					}
					$tmp['pool_type'] = $pool_type;
					$tmp['nr_disks'] = count($vdevs[0]->getDisks());
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
						$tmp['type'] = "Clone";
						$tmp['origin'] = $ds->getOrigin();
					} else {
						//This is a standard Filesystem.
						$tmp['type']= ucfirst($type);
					}
					$tmp['size'] = "n/a";
					$used = $ds->getProperty("used");
					$tmp['used'] = $used['value'];
					$available = $ds->getProperty("available");
					$tmp['available'] = $available['value'];
					$tmp['mountpoint'] = $ds->getMountPoint();
					$tmp['pool_type'] = "n/a";
					$tmp['nr_disks'] = "n/a";
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
				$vol = new OMVModuleZFSZvol();
				$tmp['size'] = $vol->getSize();
				$tmp['used'] = "n/a";
				$tmp['available'] = "n/a";
				$tmp['mountpoint'] = "n/a";
				$tmp['pool_type'] = "n/a";
				$tmp['nr_disks'] = "n/a";
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
				$tmp['size'] = "n/a";
				$tmp['used'] = "n/a";
				$tmp['available'] = "n/a";
				$tmp['mountpoint'] = "n/a";
				$tmp['pool_type'] = "n/a";
				$tmp['nr_disks'] = "n/a";
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

}


?>
