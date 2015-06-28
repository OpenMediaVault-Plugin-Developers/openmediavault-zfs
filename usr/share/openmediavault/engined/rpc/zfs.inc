<?php
/**
 * Copyright (C) 2014 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once("openmediavault/object.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/util.inc");
require_once("openmediavault/rpcservice.inc");
require_once("openmediavault/notify.inc");
require_once("/usr/share/openmediavault/engined/rpc/filesystemmgmt.inc");
require_once("/usr/share/omvzfs/Utils.php");
require_once("/usr/share/omvzfs/Dataset.php");
require_once("/usr/share/omvzfs/Snapshot.php");
require_once("/usr/share/omvzfs/Zvol.php");
require_once("/usr/share/omvzfs/Zpool.php");
require_once("/usr/share/omvzfs/NotifyListener.php");
require_once("/usr/share/omvzfs/OMVStorageZvol.php");

class OMVRpcServiceZFS extends OMVRpcServiceAbstract {
	public function getName() {
		return "ZFS";  // RPC Service name. Same as in .js files
	}

	/* Initialize the RPC service. Different methods of the RPC service are declared here*/
	public function initialize() {
		$this->registerMethod("addPool");
		$this->registerMethod("importPool");
		$this->registerMethod("exportPool");
		$this->registerMethod("getObjectTree");
		$this->registermethod("passParam");
		$this->registermethod("addObject");
		$this->registermethod("deleteObject");
		$this->registermethod("getProperties");
		$this->registermethod("setProperties");
		$this->registermethod("inherit");
		$this->registermethod("getObjectDetails");
		$this->registermethod("expandPool");
		$this->registermethod("scrubPool");
		$this->registerMethod("getCandidates");
		$this->registerMethod("rename");
	}

	public function addPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "pooltype":{"type":"string","enum":["basic","mirror",' .
					'"raidz1","raidz2","raidz3"]},
				  "force":{"type":"boolean"},
				  "mountpoint":{"type":"string"},
				  "name":{"type":"string"},
				  "devices":{"type":"string"},
				  "devalias":{"type":"string"},
				  "ashift":{"type":"boolean"},
				  "ashiftval":{"type":"string"}
			  }
		  }');
		if (preg_match('/[\s]+/', $params['name'])) {
			throw new OMVModuleZFSException("Spaces are not allowed in pool name");
		}
		switch ($params['pooltype']) {
		case "basic":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSPLAIN;
			break;
		case "mirror":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSMIRROR;
			break;
		case "raidz1":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1;
			break;
		case "raidz2":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2;
			break;
		case "raidz3":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3;
			break;
		default:
			throw new OMVModuleZFSException("Incorrect pool type specified");
			break;
		}
		//Check for user supplied options (order is important)
		$opts = "";
		if ($params['force']) {
			$opts .= "-f ";
		}
		if ($params['ashift']) {
			$opts .= "-o ashift=" . $params['ashiftval'] . " ";
		}
		if (strlen($params['mountpoint']) > 0) {
			$opts .= "-m \"" . $params['mountpoint'] . "\" ";
		}

		$disks = preg_split("/[,;]/", $params['devices']);
		foreach ($disks as $disk) {
			OMVModuleZFSUtil::setGPTLabel($disk);
		}
		switch ($params['devalias']) {
		case "path":
			try {
				if (file_exists("/dev/disk/by-path/")) {
					$tmp_disks = array();
					foreach ($disks as $disk) {
						$tmp_disks[] = OMVModuleZFSUtil::getDiskPath($disk);
					}
					$disks = $tmp_disks;
				}
			} catch (OMVModuleZFSException $e) {
				//Do nothing if an excpetion is thrown (fallback on /dev/)
			}
			break;
		case "id":
            try {
                if (file_exists("/dev/disk/by-id/")) {
                    $tmp_disks = array();
                    foreach ($disks as $disk) {
                        $tmp_disks[] = OMVModuleZFSUtil::getDiskId($disk);
                    }
                    $disks = $tmp_disks;
                }
            } catch (OMVModuleZFSException $e) {
                //Do nothing if an excpetion is thrown (fallback on /dev/)
            }
			break;
		case "dev":
			break;
		}

		$vdev = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		// Create a background process.
		$bgStatusFilename = $this->createBgProcStatus();
		$pid = $this->fork();
		if($pid > 0) { // Parent process.
			$this->initializeBgProcStatus($bgStatusFilename, $pid);
			return $bgStatusFilename;
		}
		// Child process.
		try {
			$bgOutputFilename = $this->createBgProcOutput();
			$this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);
			$pool = new OMVModuleZFSZpool($vdev, $opts);
			//Ugly fix to solve the problem of blkid not displaying info on newly created pools
			$pool->export();
			$pool->import($pool->getName());
			$this->finalizeBgProcStatus($bgStatusFilename, $output);
			exit(0);
		} catch(Exception $e) {
			$this->finalizeBgProcStatus($bgStatusFilename, "", $e);
			exit(1);
		}
	}

	public function importPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "poolname":{"type":"string"},
				  "all":{"type":"boolean"}
			  }
		}');
		if ($params['all']) {
			$cmd = "zpool import -a 2>&1";
		} elseif (strlen($params['poolname']) !== 0) {
			$cmd = "zpool import \"" . $params['poolname'] . "\" 2>&1";
		} else {
			throw new OMVModuleZFSException("Specify either pool name or to import all missing pools");
		}
		OMVModuleZFSUtil::exec($cmd,$out,$res);
	}
	
	public function exportPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		}');
		$pool = new OMVModuleZFSZpool($params['name']);
		$pool->export();
	}

	public function getObjectTree($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		$objects = OMVModuleZFSUtil::getZFSFlatArray();
		$new = array();
		foreach ($objects as $a){
			$new[$a['parentid']][] = $a;
		}
		$tree['data'] = OMVModuleZFSUtil::createTree($new, $new['root']);
		OMVModuleZFSUtil::addMissingOMVMntEnt(); //Adds missing ZFS filesystems to the OMV core
		return $tree;
	}

	public function passParam($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "key":{"type":"string"},
				  "value":{"type":"string"}
			  }
		  }');
		return array($params['key'] => $params['value']);
	}

	public function addObject($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "type":{"type":"string","enum":["filesystem","snapshot",' .
					'"volume","clone"]},
				  "path":{"type":"string"},
				  "name":{"type":"string"},
				  "size":{"type":"string"},
				  "clonename":{"type":"string"},
				  "mountpoint":{"type":"string"},
				  "thinvol":{"type":"boolean"}
				}
		  }');
		if (preg_match('/[\s]+/', $params['name'])) {
			throw new OMVModuleZFSException("Spaces are not allowed in object names");
		}
		switch ($params['type']) {
		case "clone":
			$tmp = new OMVModuleZFSSnapshot($params['path']);
			$tmp->clonesnap($params['clonename']);
			break;
		case "filesystem":
			$name = $params['path'] . "/" . $params['name'];
			$tmp = new OMVModuleZFSDataset($name);
			if (strlen($params['mountpoint']) > 0) {
				$properties = array("mountpoint"=>$params['mountpoint']);
				$tmp->setProperties($properties);
			}
			break;
		case "snapshot":
			$name = $params['path'] . "@" . $params['name'];
			$tmp = new OMVModuleZFSSnapshot($name);
			break;
		case "volume":
			$name = $params['path'] . "/" . $params['name'];
			$tmp = new OMVModuleZFSZvol($name);
			if ($params['thinvol']) {
				$tmp->create($params['size'], null, true);
			} else {
				$tmp->create($params['size'], null, false);
			}
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}
	}

	public function deleteObject($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "type":{"type":"string","enum":["Filesystem","Snapshot",' .
					'"Volume","Pool"]},
				  "name":{"type":"string"}
			  }
		  }');
		global $xmlConfig;
		$name = $params['name'];
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSDataset($name);
			OMVModuleZFSUtil::deleteShares($name);
			$mountpoint = $tmp->getMountPoint();
			$xpath = "//system/fstab/mntent[fsname='" . $name . "' and type='zfs' and dir='" . $mountpoint . "']";
			$object = $xmlConfig->get($xpath);
			$xmlConfig->delete($xpath);
			$tmp->destroy();
			break;
		case "Snapshot":
			$tmp = new OMVModuleZFSSnapshot($name);
			$tmp->destroy();
			break;
		case "Volume":
			$tmp = new OMVModuleZFSZvol($name);
			$tmp->destroy();
			break;
		case "Pool":
			$tmp = new OMVModuleZFSDataset($name);
			OMVModuleZFSUtil::deleteShares($name);
			$mountpoint = $tmp->getMountPoint();
			$xpath = "//system/fstab/mntent[fsname='" . $name . "' and type='zfs' and dir='" . $mountpoint . "']";
			$object = $xmlConfig->get($xpath);
			$xmlConfig->delete($xpath);
			$disks = OMVModuleZFSUtil::getDevDisksByPool($name);
			$tmp = new OMVModuleZFSZpool($name);
			$tmp->destroy();
			OMVModuleZFSUtil::clearZFSLabel($disks);
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}
	}

	public function getProperties($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "type":{"type":"string"},
				  "name":{"type":"string"},
				  "start":{"type":"integer"},
				  "limit":{'.$GLOBALS['OMV_JSONSCHEMA_COUNTFIELD'].'},
				  "sortfield":{'.$GLOBALS['OMV_JSONSCHEMA_SORTFIELD'].'},
				  "sortdir":{'.$GLOBALS['OMV_JSONSCHEMA_SORTDIR'].'}
			  }
		}');
		$objects = array();
		$name = $params['name'];
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSDataset($name);
			break;
		case "Snapshot":
			$tmp = new OMVModuleZFSSnapshot($name);
			break;
		case "Volume":
			$tmp = new OMVModuleZFSZvol($name);
			break;
		case "Pool":
			$tmp = new OMVModuleZFSZpool($name);
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}
		$properties = $tmp->getProperties();
		foreach ($properties as $propertyk => $propertyv) {
			if (!(strcmp($propertyv['source'], "-") == 0)) {
				$objects[] = array('property' => $propertyk,
					'value' => $propertyv['value'],
					'source' => $propertyv['source'],
					'modified' => "false");
			}
		}
		return $objects;
	}

	public function setProperties($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			"type":"object",
				"properties":{
					"type":{"type":"string","enum":["Filesystem","Snapshot",' .
						'"Volume","Pool"]},
					"name":{"type":"string"},
					"properties":{"type":"array","items":{
						"type":"object",
						"properties":{
							"property":{"type":"string"},
							"value":{"type":"string"}}}}
				}
		}');
		global $xmlConfig;
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSDataset($params['name']);
			break;
		case "Snapshot":
			$tmp = new OMVModuleZFSSnapshot($params['name']);
			break;
		case "Volume":
			$tmp = new OMVModuleZFSZvol($params['name']);
			break;
		case "Pool":
			$tmp = new OMVModuleZFSZpool($params['name']);
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}
		foreach ($params['properties'] as $property) {
			unset($objects);
			$objects = array();
			$objects[$property['property']] = $property['value'];
			$tmp->setProperties($objects);
			if ((strcmp($property['property'], "mountpoint") === 0) && (strcmp($params['type'], "Filesystem") === 0)) {
				OMVModuleZFSUtil::relocateFilesystem($params['name']);
			}
		}
	}

	public function inherit($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "type":{"type":"string","enum":["Filesystem","Snapshot",' .
					'"Volume","Pool"]},
				  "name":{"type":"string"},
				  "property":{"type":"string"}
			  }
		  }');
		// Create a background process.
		$bgStatusFilename = $this->createBgProcStatus();
		$pid = $this->fork();
		if($pid > 0) { // Parent process.
			$this->initializeBgProcStatus($bgStatusFilename, $pid);
			return $bgStatusFilename;
		}
		// Child process.
		try {
			$bgOutputFilename = $this->createBgProcOutput();
			$this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);
			switch ($params['type']) {
			case "Filesystem":
				$tmp = new OMVModuleZFSDataset($params['name']);
				break;
			case "Snapshot":
				$tmp = new OMVModuleZFSSnapshot($params['name']);
				break;
			case "Volume":
				$tmp = new OMVModuleZFSZvol($params['name']);
				break;
			case "Pool":
				$tmp = new OMVModuleZFSZpool($params['name']);
				break;
			default:
				throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
				break;
			}
			$tmp->inherit($params['property']);
			$this->finalizeBgProcStatus($bgStatusFilename, $output);
			exit(0);
		} catch(Exception $e) {
			$this->finalizeBgProcStatus($bgStatusFilename, "", $e);
			exit(1);
		}
	}

	public function getObjectDetails($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			"type":"object",
				"properties":{
					"name":{"type":"string"},
						"type":{"type":"string"}
			}
		}');
		$output = "";
		switch ($params['type']) {
		case "Filesystem":
			$output .= "Filesystem details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all \"{$params['name']}\"";
			break;
		case "Volume":
			$output .= "Volume details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all \"{$params['name']}\"";
			break;
		case "Snapshot":
			$output .= "Snapshot details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all \"{$params['name']}\"";
			break;
		case "Pool":
			$output .= "Pool status (zpool status):\n\r\n\r";
			$cmd = "zpool status \"{$params['name']}\"";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$output .= implode("\n\r", $out);
			unset($out);
			$output .= "\n\r\n\rPool details (zpool get all):\n\r\n\r";
			$cmd = "zpool get all \"{$params['name']}\"";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$output .= implode("\n\r", $out);
			unset($out);
			$output .= "\n\r\n\rPool filesystem details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all \"{$params['name']}\"";
			break;
		default:
			throw new OMVModuleZFSException("Incorrect type provided");
		}
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$output .= implode("\n\r", $out);
		return array("details" => $output);
	}

	public function expandPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "vdevtype":{"type":"string","enum":["basic","mirror",' .
					'"raidz1","raidz2","raidz3"]},
				  "name":{"type":"string"},
				  "devices":{"type":"string"},
				  "force":{"type":"boolean"},
				  "devalias":{"type":"string"}
			  }
		}');
		$pool = new OMVModuleZFSZpool($params['name']);
		switch ($params['vdevtype']) {
		case "basic":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSPLAIN;
			break;
		case "mirror":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSMIRROR;
			break;
		case "raidz1":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1;
			break;
		case "raidz2":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2;
			break;
		case "raidz3":
			$pooltype = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3;
			break;
		default:
			throw new OMVModuleZFSException("Incorrect pool type specified");
			break;
		}
		if ($params['force']) {
			$opts .= "-f ";
		}
		$disks = preg_split("/[,;]/", $params['devices']);
		foreach ($disks as $disk) {
			OMVModuleZFSUtil::setGPTLabel($disk);
		}
        switch ($params['devalias']) {
        case "path":
            try {
                if (file_exists("/dev/disk/by-path/")) {
                    $tmp_disks = array();
                    foreach ($disks as $disk) {
                        $tmp_disks[] = OMVModuleZFSUtil::getDiskPath($disk);
                    }
                    $disks = $tmp_disks;
                }
            } catch (OMVModuleZFSException $e) {
                //Do nothing if an excpetion is thrown (fallback on /dev/)
            }
            break;
        case "id":
            try {
                if (file_exists("/dev/disk/by-id/")) {
                    $tmp_disks = array();
                    foreach ($disks as $disk) {
                        $tmp_disks[] = OMVModuleZFSUtil::getDiskId($disk);
                    }
                    $disks = $tmp_disks;
                }
            } catch (OMVModuleZFSException $e) {
                //Do nothing if an excpetion is thrown (fallback on /dev/)
            }
            break;
        case "dev":
            break;
        }

		$vdev[] = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		// Create a background process.
		$bgStatusFilename = $this->createBgProcStatus();
		$pid = $this->fork();
		if($pid > 0) { // Parent process.
			$this->initializeBgProcStatus($bgStatusFilename, $pid);
			return $bgStatusFilename;
		}
		// Child process.
		try {
			$bgOutputFilename = $this->createBgProcOutput();
			$this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);
			$pool->addVdev($vdev, $opts);
			//Ugly fix to solve the problem of blkid not displaying info on newly created pools
			$pool->export();
			$pool->import($pool->getName());
			$this->finalizeBgProcStatus($bgStatusFilename, $output);
			exit(0);
		} catch(Exception $e) {
			$this->finalizeBgProcStatus($bgStatusFilename, "", $e);
			exit(1);
		}
	}

	public function scrubPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		}');
        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if($pid > 0) { // Parent process.
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }
        // Child process.
        try {
			$pool = new OMVModuleZFSZpool($params['name']);
			$pool->scrub();
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
	}

	public function getCandidates($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			"role" => OMV_ROLE_ADMINISTRATOR
		));
		$fs = new OMVRpcServiceFileSystemMgmt();
		$objects = $fs->getCandidates($params, $context);
		$devs = array();
		foreach ($objects as $object) {
			if (!(preg_match('/zd[0-9]+(p\d+)?$/', $object['devicefile']))) {
				$devs[] = $object;
			}
		}
		return $devs;
	}

	public function rename($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "newname":{"type":"string"},
				  "oldname":{"type":"string"},
				  "type":{"type":"string"},
				  "oldpath":{"type":"string"}
			  }
		}');
		global $xmlConfig;
		
		preg_match('/^(.*)' . $params['oldname'] . '$/', $params['oldpath'], $matches);
		$pre = "";
		if (isset($matches[1])) {
			$pre = $matches[1];
		}
		$newpath = $pre . $params['newname'];

		switch($params['type']) {
		case "Filesystem":
			//Rename the object first
			$ds = new OMVModuleZFSDataset($params['oldpath']);
			$ds->rename($newpath);

			//Locate all filesystems with the oldpath as subpath
			$xpath = "//system/fstab/mntent[type='zfs']";
			$objects = $xmlConfig->getList($xpath);
			foreach($objects as $objectk => &$objectv) {
				if(preg_match('/^' . preg_quote($params['oldpath'], "/") . '([\/]?.*)$/', $objectv['fsname'], $matches)) {
					$post = "";
					if (isset($matches[1])) {
						$post = $matches[1];
					}
					unset($matches);
					preg_match('/^(.*)' . $params['oldname'] . '$/', $params['oldpath'], $matches);
					$newpath = $matches[1] . $params['newname'] . $post;
					unset($matches);
					$objectv['fsname'] = $newpath;
					$objectv['dir'] = "/" . $newpath;
					$xmlConfig->replace("//system/fstab/mntent[uuid='" . $objectv['uuid'] . "']", $objectv);
				}
			}
			break;

		case "Volume":
			$vol = new OMVModuleZFSZvol($params['oldpath']);
			$vol->rename($newpath);
			break;
		
		case "Snapshot":
			$snap = new OMVModuleZFSSnapshot($params['oldpath']);
			$snap->rename($newpath);
			break;

		default:
			break;
		}
	}
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance(); // Get the "root" instance for the Services
$rpcServiceMgr->registerService(new OMVRpcServiceZFS()); // Register a new instance of the RPC service described above
