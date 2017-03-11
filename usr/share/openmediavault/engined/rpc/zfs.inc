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

use OMV\Config\Database;
use OMV\Config\ConfigObject;
use OMV\Exception;
use OMV\Rpc\Rpc;
use OMV\Rpc\ServiceAbstract;
use OMV\Engine\Notify\Dispatcher;
require_once("/usr/share/omvzfs/Utils.php");
require_once("/usr/share/omvzfs/Snapshot.php");
require_once("/usr/share/omvzfs/Zvol.php");
require_once("/usr/share/omvzfs/Zpool.php");
require_once("/usr/share/omvzfs/Filesystem.php");

class OMVRpcServiceZFS extends ServiceAbstract {
	private $dataModelPath = "conf.service.zfs";

	public function __construct(Database $database = null, Dispatcher $dispatcher = null){
		$this->database = $database ? $database : Database::getInstance();
		$this->dispatcher = $dispatcher ? $dispatcher : Dispatcher::getInstance();
	}

	public function getName() {
		return "zfs";
	}

	/* Initialize the RPC service. Different methods of the RPC service are declared here*/
	public function initialize() {
		$this->registerMethod("addPool");
		$this->registerMethod("importPool");
		$this->registerMethod("exportPool");
		$this->registerMethod("getObjectTree");
		$this->registerMethod("getAllSnapshots");
		$this->registerMethod("addObject");
		$this->registerMethod("deleteObject");
		$this->registerMethod("getProperties");
		$this->registerMethod("setProperties");
		$this->registerMethod("inherit");
		$this->registerMethod("getObjectDetails");
		$this->registerMethod("expandPool");
		$this->registerMethod("scrubPool");
		$this->registerMethod("getCandidates");
		$this->registerMethod("rename");
		$this->registerMethod("getSettings");
		$this->registerMethod("setSettings");
		$this->registerMethod("rollbackSnapshot");
        $this->registerMethod("set");
	}

    public function set($params, $context) {
        // empty function for details window
        // TODO:  change window type for details
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
					$tmp_disks = [];
					foreach ($disks as $disk) {
						$tmp_disks[] = OMVModuleZFSUtil::getDiskPath($disk);
					}
					$disks = $tmp_disks;
				}
			} catch (Exception $e) {
				//Do nothing if an excpetion is thrown (fallback on /dev/)
			}
			break;
		case "id":
            try {
                if (file_exists("/dev/disk/by-id/")) {
                    $tmp_disks = [];
                    foreach ($disks as $disk) {
                        $tmp_disks[] = OMVModuleZFSUtil::getDiskId($disk);
                    }
                    $disks = $tmp_disks;
                }
            } catch (Exception $e) {
                //Do nothing if an excpetion is thrown (fallback on /dev/)
            }
			break;
		case "dev":
			break;
		}

		$vdev = new OMVModuleZFSVdev($params['name'], $pooltype, $disks);
		$pool = OMVModuleZFSZpool::create($vdev, $opts);

	}

	public function importPool($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "poolname":{"type":"string"},
				  "all":{"type":"boolean"},
				  "force":{"type":"boolean"}
			  }
		}');

		$cmd = "zpool import ";
		if ($params['force'])
			$cmd .= "-f ";
		if ($params['all'])
			$cmd .= "-a ";
		else if (strlen($params['poolname']) !== 0)
			$cmd .= "\"" . $params['poolname'] . "\" ";
		$cmd .= "2>&1";

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
		$new = [];
		foreach ($objects as $a){
			$new[$a['parentid']][] = $a;
		}
		$tree['data'] = OMVModuleZFSUtil::createTree($new, $new['root']);
		OMVModuleZFSUtil::fixOMVMntEnt($context); //Adds missing ZFS filesystems to the OMV core
		return $tree;
	}

	public function getAllSnapshots($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		$this->validateMethodParams($params, "rpc.common.getlist");
		$objects = OMVModuleZFSUtil::getAllSnapshots();
		return $this->applyFilter($objects, $params['start'],
		  $params['limit'], $params['sortfield'], $params['sortdir']);
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
			$tmp = OMVModuleZFSFilesystem::create($name);
			if (strlen($params['mountpoint']) > 0) {
				$properties = array("mountpoint"=>$params['mountpoint']);
				$tmp->setProperties($properties);
			}
			$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
			  "org.openmediavault.storage.zfs.filesystem", $context);
			break;
		case "snapshot":
			$name = $params['path'] . "@" . $params['name'];
			OMVModuleZFSSnapshot::create($name);
			break;
		case "volume":
			$name = $params['path'] . "/" . $params['name'];
			if ($params['thinvol']) {
				OMVModuleZFSZvol::create($name,$params['size'], null, true);
			} else {
				OMVModuleZFSZvol::create($name,$params['size'], null, false);
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
		$name = $params['name'];

		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSFilesystem($name);
			if(OMVModuleZFSUtil::checkOMVShares($context,$tmp))
				throw new OMVModuleZFSException("Before deleting this filesystem, You must delete shares referencing this filesystem.");
			OMVModuleZFSUtil::deleteOMVMntEnt($context,$tmp);
			$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
			  "org.openmediavault.storage.zfs.filesystem", $context);
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
			$tmp = new OMVModuleZFSZpool($name);
			if(OMVModuleZFSUtil::checkOMVShares($context,$tmp))
				throw new OMVModuleZFSException("Before deleting this filesystem, You must delete shares referencing this filesystem.");
			OMVModuleZFSUtil::deleteOMVMntEnt($context,$tmp);
			$disks = $tmp->getDevDisks();
			OMVModuleZFSZpool::clearZFSLabel($disks);
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
		$objects = [];
		$name = $params['name'];
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSFilesystem($name);
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
		$tmp->updateAllProperties();
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
		switch ($params['type']) {
		case "Filesystem":
			$tmp = new OMVModuleZFSFilesystem($params['name']);
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
			$objects = [];
			$objects[$property['property']] = $property['value'];
			$tmp->setProperties($objects);
			if ((strcmp($property['property'], "mountpoint") === 0)) {
				OMVModuleZFSUtil::relocateOMVMntEnt($context,$tmp);
				$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
					"org.openmediavault.conf.system.sharedfolder", $context);
			}
		}

		// If we updated a dataset, we should notify.
		if($tmp instanceof OMVModuleZFSFilesystem) {
			$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
			  "org.openmediavault.storage.zfs.filesystem", $context);
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
				$tmp = new OMVModuleZFSFilesystem($params['name']);
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
                    $tmp_disks = [];
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
                    $tmp_disks = [];
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
		$pool->addVdev($vdev, $opts);
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
		$pool = new OMVModuleZFSZpool($params['name']);
		$process = $pool->scrub();
	}

	public function getCandidates($params, $context) {
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			"role" => OMV_ROLE_ADMINISTRATOR
		));
		$fs = new OMVRpcServiceFileSystemMgmt();
		$objects = $fs->getCandidates($params, $context);
		$devs = [];
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


		preg_match('/^(.*)' . $params['oldname'] . '$/', $params['oldpath'], $matches);
		$pre = "";
		if (isset($matches[1])) {
			$pre = $matches[1];
		}
		$newpath = $pre . $params['newname'];

		switch($params['type']) {
		case "Filesystem":
			//Rename the object first
			$ds = new OMVModuleZFSFilesystem($params['oldpath']);
			$ds->rename($newpath);
			renameOMVMntEnt($context,$ds);
			$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
			  "org.openmediavault.storage.zfs.filesystem", $context);
			break;

		case "Volume":
			$vol = new OMVModuleZFSZvol();
			$vol->load($params['oldpath']);
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

	/**
	 * Get the general ZFS configuration settings.
	 *
	 * @param params The method params
	 * @param context The context of the caller.
	 * @return A JSON object containing the fields required
	 *	to configure Kerberos.
	 * @throw E_CONFIG_GET_OBJECT_FAILED
	 */
	function getSettings($params, $context) {


		// Validate the RPC caller context.
		$this -> validateMethodContext($context, array(
				"role" => OMV_ROLE_ADMINISTRATOR
		));

		// Get the configuration object.
		$object = $this->database->getAssoc($this->dataModelPath);

		// Modify result object.
		$object['autoShareNestedEnabled'] = boolvalEx($object['autoShareNestedEnabled']);

		return $object;
	}

	/**
	 * Set the general ZFS configuration settings.
	 *
	 * @param params An array containing all of the fields and values.
	 * @param context The context of the caller.
	 * @return The stored configuration object.
	 * @throw E_CONFIG_SET_OBJECT_FAILED
	 */
	function setSettings($params, $context) {

		// Validate the RPC caller context
		$this -> validateMethodContext($context, array(
			"role" => OMV_ROLE_ADMINISTRATOR
		));

		// Validate the parameters of the RPC servicemethod.
		$this -> validateMethodParams($params, '{
			"type": "object",
			"properties": {
				"autoShareNestedEnabled": {"type": "boolean"},
				"autoShareNestedProperty": {"type": "string", "optional": true}
			}
		}');

		// Prepare the configuration object.
		$object = array(
			"autoShareNestedEnabled" => array_boolval($params, 'autoShareNestedEnabled'),
			"autoShareNestedProperty" => trim($params['autoShareNestedProperty'])
		);

		$config = new ConfigObject($this->dataModelPath);
		$config->setAssoc($object);
		$this->database->set($config);

		// Notify NFS since this may change what is exported
		$this->dispatcher->notify(OMV_NOTIFY_MODIFY,
		  "org.openmediavault.storage.zfs.filesystem", $context);

		return $object;
	}

	public function rollbackSnapshot($params, $context) {
		$this->validateMethodContext($context, array("role" => OMV_ROLE_ADMINISTRATOR));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		}');

		$snap = new OMVModuleZFSSnapshot($params['name']);
		$snap->rollback();

		return $snap;
	}
}
