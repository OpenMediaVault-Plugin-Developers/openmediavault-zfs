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
require_once("/usr/share/omvzfs/Utils.php");
require_once("/usr/share/omvzfs/Dataset.php");
require_once("/usr/share/omvzfs/Snapshot.php");
require_once("/usr/share/omvzfs/Zvol.php");
require_once("/usr/share/omvzfs/Zpool.php");
require_once("/usr/share/omvzfs/NotifyListener.php");

class OMVRpcServiceZFS extends OMVRpcServiceAbstract {
	public function getName() {
		return "ZFS";  // RPC Service name. Same as in .js files
	}

	/* Initialize the RPC service. Different methods of the RPC service are declared here*/
	public function initialize() {
		$this->registerMethod("addPool");
		$this->registerMethod("getObjectTree");
		$this->registermethod("passParam");
		$this->registermethod("addObject");
		$this->registermethod("deleteObject");
		$this->registermethod("getProperties");
		$this->registermethod("setProperties");
		$this->registermethod("inherit");
		$this->registermethod("getSharedParams");
		$this->registermethod("createShare");
		$this->registermethod("getObjectDetails");
		$this->registermethod("expandPool");
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
				  "diskpath":{"type":"boolean"}
			  }
		  }');
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
		//Check for user supplied options
		$opts = "";
		if ($params['force']) {
			$opts .= "-f ";
		}
		if (strlen($params['mountpoint']) > 0) {
			$opts .= "-m " . $params['mountpoint'] . " ";
		}

		$disks = preg_split("/[,;]/", $params['devices']);
		foreach ($disks as $disk) {
			OMVModuleZFSUtil::setGPTLabel($disk);
		}
		//Use /dev/disk/by-path as suggested in ZoL FAQ.
		if ($params['diskpath']) {
			try {
				if (file_exists("/dev/disk/by-path/")) {
					$tmp_disks = array();
					foreach ($disks as $disk) {
						$tmp_disks[] = OMVModuleZFSUtil::getDiskPath($disk);
					}
					$disks = $tmp_disks;
				}
			} catch (OMVModuleZFSException $e) {
				//Do nothing if an excpetion is thrown
			}
		}

		$vdev = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		$pool = new OMVModuleZFSZpool($vdev, $opts);
		//Ugly fix to solve the problem of blkid not displaying info on newly created pools
		$pool->export();
		$pool->import($pool->getName());
	}

	public function getObjectTree($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		$objects = OMVModuleZFSUtil::getZFSFlatArray();
		$new = array();
		foreach ($objects as $a){
			$new[$a['parentid']][] = $a;
		}
		$tree = OMVModuleZFSUtil::createTree($new, $new['root']);
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
				  "mountpoint":{"type":"string"}
			  }
		  }');
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
			$tmp->create($params['size']);
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
			OMVModuleZFSUtil::deleteShares($name);
			$tmp = new OMVModuleZFSDataset($name);
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
			$disks = OMVModuleZFSUtil::getDevDisksByPool($name);
			$pooluuid = OMVModuleZFSUtil::getUUIDbyName($name);
			$tmp = new OMVModuleZFSZpool($name);
			$tmp->destroy();
			$xpath = "//system/fstab/mntent[fsname='" . $pooluuid . "' and type='zfs']";
			$object = $xmlConfig->get($xpath);
			$xmlConfig->delete($xpath);
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

	public function getSharedParams($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "type":{"type":"string"},
				  "name":{"type":"string"}
			  }
		  }');
		$objects = array();
		//$ds = new OMVModuleZFSDataset($params['name']);
		//$mountpoint = $ds->getMountPoint();
		return array(
			//"mountpoint" => $mountpoint,
			"name" => $params['name'],
			"type" => $params['type']);
	}

	public function createShare($params, $context) {
		global $xmlConfig;
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"},
				  "type":{"type":"string","enum":["Filesystem"]},
				  "sharename":{'.$GLOBALS['OMV_JSONSCHEMA_SHARENAME'].'},
				  "comment":{"type":"string"},
				  "mode":{"type":"string","enum":["700","750","755",'.
				  	'"770","775","777"],"optional":true},
				  "mountpoint":{"type":"string"}
			  }
		  }');

   		// The field 'reldirpath' may not contain the characters '..'. This
	    // is because of security reasons: the given canonicalized absolute
	    // path MUST be below the given mount point.
	    if(1 == preg_match("/\.\./", $params['mountpoint'])) {
	    	throw new OMVException(OMVErrorMsg::E_RPC_SERVICE_METHOD_INVALID_PARAMS,
				sprintf(gettext("The field '%s' contains forbidden two-dot symbols"), "mountpoint"));
		}

		// Prepare the configuration object. Use the name of the shared
		// folder as the relative directory name of the share.
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSDataset($params['name']);
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}

		$poolname = OMVModuleZFSUtil::getPoolname($params['name']);
		$pooluuid = OMVModuleZFSUtil::getUUIDbyName($poolname);
		$dir = $tmp->getMountPoint();

		//Get the mntent object and fetch it's uuid.
		$xpath = "//system/fstab/mntent[fsname='" . $pooluuid . "' and dir='" . $dir . "' and type='zfs']";
		$mountpoint = $xmlConfig->get($xpath);
		$mntentref = $mountpoint['uuid'];

		$uuid = OMVUtil::uuid();
		$pathName = $dir . "/" . trim($params['mountpoint'], "/");
		$reldirpath = trim($params['mountpoint'], "/");
		$object = array(
			"uuid" => $uuid,
			"name" => $params['sharename'],
			"comment" => $params['comment'] . "*** ZFS share on " . $params['name'] . " ***",
			"mntentref" => $mntentref,
			"reldirpath" => $reldirpath
		);

		// Set the configuration object.
		$success = FALSE;
		// Check uniqueness. The share name must be global unique because
		// the name is also used when exporting a shared folder via NFS for
		// example.
		$xpath = sprintf("//system/shares/sharedfolder[name='%s']",
			$params['name']);
		if(TRUE === $xmlConfig->exists($xpath)) {
			throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
				gettext("A shared folder with the given name already exists"));
		}

		// Add empty list of privileges per default.
		$object['privileges'] = array();

		// Append object to configuration.
		$success = $xmlConfig->set("//system/shares",
			array("sharedfolder" => $object));
		if(FALSE === $success) {
			throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
		}

		// Append the file mode field to the notification object if set.
		// Defaults to 775.
		$object['mode'] = "775";
		if(array_key_exists("mode", $params)) {
			$object['mode'] = $params['mode'];
		}

		// Create the shared folder directory if necessary.
		if(FALSE === file_exists($pathName)) {
		// Create the directory. Note, the function seems to have a bug
		// when using the mask parameter. E.g. octdec("777") does not
		// create the correct permissions as expected, thus change the
		// mode using chmod.
			if(FALSE === mkdir($pathName, 0700, TRUE)) {
				throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
					sprintf("Failed to create the directory '%s'", $pathName));
			}
			// Change the directory mode.
			if(FALSE === chmod($pathName, octdec($object['mode']))) {
				throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
					sprintf("Failed to set file mode to '%s' for '%s'",
					$object['mode'], $pathName));
			}
		}

		// Change group owner of directory to configured default group,
		// e.g. 'users'.
		if(FALSE === chgrp($pathName, $GLOBALS['OMV_USERMGMT_DEFAULT_GROUP'])) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
				sprintf("Failed to set file group to '%s' for '%s'",
				$GLOBALS['OMV_USERMGMT_DEFAULT_GROUP'], $pathName));
		}

		// Set the setgid bit. Setting this permission means that all files
		// created in the folder will inherit the group of the folder rather
		// than the primary group of the user who creates the file.
		$mode = fileperms($pathName) | 02000;
		if(FALSE === chmod($pathName, $mode)) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
				sprintf("Failed to set file mode to '%o' for '%s'",
				$mode, $pathName));
		}

		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_CREATE,"org.openmediavault.system.shares.sharedfolder", $object);
		// Return the configuration object.
		return $object;
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
			$cmd = "zfs get all {$params['name']}";
			break;
		case "Volume":
			$output .= "Volume details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all {$params['name']}";
			break;
		case "Snapshot":
			$output .= "Snapshot details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all {$params['name']}";
			break;
		case "Pool":
			$output .= "Pool status (zpool status):\n\r\n\r";
			$cmd = "zpool status {$params['name']}";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$output .= implode("\n\r", $out);
			unset($out);
			$output .= "\n\r\n\rPool details (zpool get all):\n\r\n\r";
			$cmd = "zpool get all {$params['name']}";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$output .= implode("\n\r", $out);
			unset($out);
			$output .= "\n\r\n\rPool filesystem details (zfs get all):\n\r\n\r";
			$cmd = "zfs get all {$params['name']}";
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
				  "diskpath":{"type":"boolean"}
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
		//Use /dev/disk/by-path as suggested in ZoL FAQ.
		if ($params['diskpath']) {
			try {
				if (file_exists("/dev/disk/by-path/")) {
					$tmp_disks = array();
					foreach ($disks as $disk) {
						$tmp_disks[] = OMVModuleZFSUtil::getDiskPath($disk);
					}
					$disks = $tmp_disks;
				}
			} catch (OMVModuleZFSException $e) {
				//Do nothing if an exception is thrown
			}
		}
		$vdev[] = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		$pool->addVdev($vdev, $opts);
		//Ugly fix to solve the problem of blkid not displaying info on newly created pools
		$pool->export();
		$pool->import($pool->getName());
	}
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance(); // Get the "root" instance for the Services
$rpcServiceMgr->registerService(new OMVRpcServiceZFS()); // Register a new instance of the RPC service described above
