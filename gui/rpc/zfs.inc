<?php

require_once("openmediavault/object.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/util.inc");
require_once("openmediavault/rpcservice.inc");
require_once("openmediavault/notify.inc");
require_once("zfs/Utils.php");
require_once("zfs/Dataset.php");
require_once("zfs/Snapshot.php");
require_once("zfs/Zvol.php");
require_once("zfs/Zpool.php");
require_once("zfs/NotifyListener.php");

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
	}

	public function addPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
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
		$disks = preg_split("/[,;]/", $params['devices']);
		$vdev = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		$pool = new OMVModuleZFSZpool($vdev);
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
		//$msg = "Key=" . $params['key'] . ";Value=" . $params['value'] . ";";
		//throw new OMVModuleZFSException($msg);
		return array($params['key'] => $params['value']);
	}

	public function addObject($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		switch ($params['type']) {
		case "filesystem":
			$name = $params['path'] . "/" . $params['name'];
			$tmp = new OMVModuleZFSDataset($name);
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
		switch ($params['type']) {
		case "Filesystem":
		case "Clone":
			$name = $params['name'];
			$tmp = new OMVModuleZFSDataset($name);
			$tmp->destroy();
			break;
		case "Snapshot":
			$name = $params['name'];
			$tmp = new OMVModuleZFSSnapshot($name);
			$tmp->destroy();
			break;
		case "Volume":
			$name = $params['name'];
			$tmp = new OMVModuleZFSZvol($name);
			$tmp->destroy();
			break;
		case "Pool":
			$name = $params['name'];
			$tmp = new OMVModuleZFSZpool($name);
			$tmp->destroy();
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}
	}

	public function getProperties($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		$objects = array();
		$name = $params['name'];
		switch ($params['type']) {
		case "Filesystem":
		case "Clone":
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
		$objects = array();
		switch ($params['type']) {
		case "Filesystem":
		case "Clone":
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
			$objects[$property['property']] = $property['value'];
		}
		$tmp->setProperties($objects);
	}

	public function inherit($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
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
			case "Clone":
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
		$objects = array();
		$ds = new OMVModuleZFSDataset($params['name']);
		$mountpoint = $ds->getMountPoint();
		return array(
			"mountpoint" => $mountpoint,
			"name" => $params['name'],
			"type" => $params['type']);
	}

	public function createShare($params, $context) {
		global $xmlConfig;
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));

		//Get the UUID of the Pool
		$pooluuid = OMVModuleZFSUtil::getUUIDbyName($params['name']);
		preg_match('/^([A-Za-z0-9]+)\/?.*$/', $params['name'], $result);
		$poolname = $result[1];
		unset($result);

		//Get the mntent object and fetch it's uuid.
		$xpath = "//system/fstab/mntent[fsname=" . $pooluuid . "]";
		$object = $xmlConfig->get($xpath);
		$mntentref = $object['uuid'];

		// Prepare the configuration object. Use the name of the shared
		// folder as the relative directory name of the share.
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSDataset($params['name']);
			break;
		case "Clone":
			$tmp = new OMVModuleZFSDataset($params['name']);
			break;
		default:
			throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
			break;
		}

		$uuid = OMVUtil::uuid();
		$pathName = $tmp->getMountPoint();
		$subdirs = preg_split('/\//',$pathName);
		$reldirpath = $subdirs[count($subdirs)-1];
		$object = array(
			"uuid" => $uuid,
			"name" => $params['sharename'],
			"comment" => $params['comment'],
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

}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance(); // Get the "root" instance for the Services
$rpcServiceMgr->registerService(new OMVRpcServiceZFS()); // Register a new instance of the RPC service described above
?>

