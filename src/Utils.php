<?php
require_once("Exception.php");
require_once("openmediavault/util.inc");
require_once("Dataset.php");

/**
 * Helper class for ZFS module
 */
class OMVModuleZFSUtil {

	/**
	 * Get an array with all ZFS objects
	 *
	 * @return An array with all ZFS objects
	 */
	public static function getZFSFlatArray() {
		$prefix = "root/pool-";
		$objects = array();
		$cmd = "zfs list -H -t all -o name,type 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		foreach ($out as $line) {
			$parts = preg_split('/\t/',$line);
			if ((strpos($parts[0],'/') === false) && (strpos($parts[0],'@') === false)) {
				$tmp = array('id'=>$prefix . $parts[0],
					'parentid'=>'root',
					'name'=>$parts[0],
					'type'=>'Pool',
					'icon'=>'images/raid.png',
					'expanded'=>true,
					'path'=>$parts[0]);
				array_push($objects,$tmp);
				$tmp = array('id'=>$prefix . $parts[0] . '/' . $parts[0],
					'parentid'=>$prefix . $parts[0],
					'name'=>$parts[0],
					'type'=>'Filesystem',
					'icon'=>'images/filesystem.png',
					'path'=>$parts[0],
					'expanded'=>true);
				array_push($objects,$tmp);
			} elseif (strpos($parts[0],'/') === false) {
				$pname = preg_split('/\@/',$parts[0]);
				$tmp = array('id'=>$prefix . $pname[0] . '/' . $parts[0],
					'parentid'=>$prefix . $pname[0]. '/' . $pname[0],
					'name'=>$pname[1],
					'type'=>'Snapshot',
					'icon'=>'images/zfs_snap.png',
					'path'=>$parts[0],
					'expanded'=>true);
				array_push($objects,$tmp);
			} elseif (preg_match('/(.*)\@(.*)$/', $parts[0], $result)) {
				$pname = preg_split('/\//',$parts[0]);
				$id = $prefix . $pname[0] . "/" . $result[0];
				$parentid = $prefix . $pname[0] . "/" . $result[1];
				$name = $result[2];
				$type = "Snapshot";
				$icon = "images/zfs_snap.png";
				$tmp = array('id'=>$id,
					'parentid'=>$parentid,
					'name'=>$name,
					'type'=>$type,
					'icon'=>$icon,
					'path'=>$parts[0],
					'expanded'=>true);
				array_push($objects,$tmp);
			} elseif (preg_match('/(.*)\/(.*)$/', $parts[0], $result)) {
				$pname = preg_split('/\//',$parts[0]);
				$id = $prefix . $pname[0] . "/" . $result[0];
				$parentid = $prefix . $pname[0] . "/" . $result[1];
				$name = $result[2];
				$type = ucfirst($parts[1]);
				if (strcmp($type, "Filesystem") == 0) {
					$icon = "images/filesystem.png";
				} else {
					$icon = "images/zfs_disk.png";
				}
				$tmp = array('id'=>$id,
					'parentid'=>$parentid,
					'name'=>$name,
					'type'=>$type,
					'icon'=>$icon,
					'path'=>$parts[0],
					'expanded'=>true);
				array_push($objects,$tmp);
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
