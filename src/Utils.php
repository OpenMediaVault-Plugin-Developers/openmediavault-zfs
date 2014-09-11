<?php
require_once("Exception.php");
require_once("openmediavault/util.inc");
require_once("Dataset.php");

/**
 * Helper class for ZFS module
 */
class OMVModuleZFSUtil {

	/**
	 * Get UUID of ZFS pool by name
	 *
	 * @return string UUID of the pool
	 */
	public static function getUUIDbyName($name) {
		preg_match('/^([A-Za-z0-9]+)\/?.*$/', $name, $result);
		$name = $result[1];
		unset($result);
		$cmd = "zpool get guid " . $name . " 2>&1";
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
	 * Add any missing ZFS pool to the OMV backend
	 *
	 */
	public static function addMissingOMVMntEnt() {
		global $xmlConfig;
		$msg = "";
		$cmd = "zpool list -H -o name";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		foreach($out as $name) {
			$pooluuid = OMVModuleZFSUtil::getUUIDbyName($name);
			if (isset($pooluuid)) {
				$pooluuid = "UUID=" . $pooluuid;
				$xpath = "//system/fstab/mntent";
				$object = $xmlConfig->get($xpath);
				$uuidexists = false;
				foreach ($object as $obj) {
					if (strcmp($pooluuid, $obj['fsname']) === 0) {
						$uuidexists = true;
						break;
					}
				}
				if (!$uuidexists) {
					$uuid = OMVUtil::uuid();
					$ds = new OMVModuleZFSDataset($name);
					$dir = $ds->getMountPoint();
					$object = array(
						"uuid" => $uuid,
						"fsname" => $pooluuid,
						"dir" => $dir,
						"type" => "zfs",
						"opts" => "rw,relatime,xattr",
						"freq" => "0",
						"passno" => "2"
					);
					$xmlConfig->set("//system/fstab",array("mntent" => $object));
					$dispatcher = &OMVNotifyDispatcher::getInstance();
					$dispatcher->notify(OMV_NOTIFY_CREATE,"org.openmediavault.system.fstab.mntent", $object);
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
					//This is a Pool, thus create both the Pool entry and a Filesystem entry corresponding to the Pool.
					$tmp = array('id'=>$prefix . $path,
						'parentid'=>'root',
						'name'=>$path,
						'type'=>'Pool',
						'icon'=>'images/raid.png',
						'expanded'=>$expanded,
						'path'=>$path);
					array_push($objects,$tmp);
					$tmp = array('id'=>$prefix . $path . '/' . $path,
						'parentid'=>$prefix . $path,
						'name'=>$path,
						'type'=>'Filesystem',
						'icon'=>'images/filesystem.png',
						'path'=>$path,
						'expanded'=>$expanded);
					array_push($objects,$tmp);
				} else {
					//This is a Filesystem other than the Pool
					preg_match('/(.*)\/(.*)$/', $path, $result);
					$tmp = array('id'=>$prefix . $root . "/" . $path,
						'parentid'=>$prefix . $root . "/" . $result[1],
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
					array_push($objects,$tmp);
				}
				break;

			case "volume":
				preg_match('/(.*)\/(.*)$/', $path, $result);
				$tmp = array('id'=>$prefix . $root . "/" . $path,
					'parentid'=>$prefix . $root . "/" . $result[1],
					'name'=>$result[2],
					'type'=>ucfirst($type),
					'icon'=>"images/zfs_disk.png",
					'path'=>$path,
					'expanded'=>$expanded);
				array_push($objects,$tmp);
				break;

			case "snapshot":
				preg_match('/(.*)\@(.*)$/', $path, $result);
				$subdirs = preg_split('/\//',$result[1]);
				$root = $subdirs[0];
				$tmp = array('id'=>$prefix . $root . "/" . $path,
					'parentid'=>$prefix . $root . "/" . $result[1],
					'name'=>$result[2],
					'type'=>ucfirst($type),
					'icon'=>'images/zfs_snap.png',
					'path'=>$path,
					'expanded'=>$expanded);
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
