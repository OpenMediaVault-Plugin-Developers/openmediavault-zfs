<?php
/**
 * Copyright (C) 2014-2026 openmediavault plugin developers
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
require_once("/usr/share/omvzfs/VdevState.php");

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
    public function initialize()
    {
        $this->registerMethod("doDiscover");
        $this->registerMethod("doDiscoverBg");
        $this->registerMethod("addPool");
        $this->registerMethod("importPool");
        $this->registerMethod("exportPool");
        $this->registerMethod("getObjectTree");
        $this->registerMethod("listCompressionTypes");
        $this->registerMethod("listPools");
        $this->registerMethod("listPoolsBg");
        $this->registerMethod("getAllSnapshots");
        $this->registerMethod("getAllSnapshotsBg");
        $this->registerMethod("addObject");
        $this->registerMethod("addObjectBg");
        $this->registerMethod("deleteObject");
        $this->registerMethod("deleteObjectBg");
        $this->registerMethod("getProperties");
        $this->registerMethod("setProperties");
        $this->registerMethod("inherit");
        $this->registerMethod("getObjectDetails");
        $this->registerMethod("expandPool");
        $this->registerMethod("scrubPool");
        $this->registerMethod("getCandidates");
        $this->registerMethod("getEmptyCandidates");
        $this->registerMethod("rename");
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("rollbackSnapshot");
        $this->registerMethod("set");
        $this->registerMethod("getStats");
        $this->registerMethod("fixPackages");
    }

    public function set($params, $context)
    {
        // empty function for details window
        // TODO:  change window type for details
    }

    public function doDiscover($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
        // Adds missing ZFS filesystems to the OMV core
        OMVModuleZFSUtil::fixOMVMntEnt($context);
    }

    public function doDiscoverBg($params, $context)
    {
        return $this->callMethodBg("doDiscover", $params, $context);
    }

    public function getStats($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $arcstats = file("/proc/spl/kstat/zfs/arcstats");

        foreach($arcstats as $line) {
          if (strpos($line,"hits") === 0) {
            $parts = preg_split('/\s+/', $line);
            $hits = $parts[2];
          }
          if (strpos($line,"misses") === 0) {
            $parts = preg_split('/\s+/', $line);
            $misses = $parts[2];
          }
          if (strpos($line,"size") === 0) {
            $parts = preg_split('/\s+/', $line);
            $size = $parts[2];
          }
        }
        if ($hits > 0 || $misses > 0) {
            $stats = [
              "hits" => $hits,
              "misses" => $misses,
              "hitsMisses" => $hits + $misses,
              "ratio" => ($hits / ($hits + $misses)) * 100,
              "size" => $size
            ];
        } else {
            $stats = [
              "hits" => 0,
              "misses" => 0,
              "hitsMisses" => 0,
              "ratio" => 0,
              "size" => 0
            ];
        }
        return ($stats);
    }

    public function addPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.addpool');

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
            $opts .= sprintf("-o ashift=%s ", $params['ashiftval']);
        }
        $opts .= "-o failmode=continue ";
        $opts .= "-o autoexpand=on ";
        $opts .= "-O atime=off ";
        $opts .= "-O acltype=posix ";
        $opts .= "-O xattr=sa ";
        if (strlen($params['mountpoint']) > 0) {
            $opts .= sprintf('-m "%s" ', $params['mountpoint']);
        }

        //$disks = preg_split("/[,;]/", $params['devices']);
        $disks = $params['devices'];

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
        //$tmp = new OMVModuleZFSZpool($params['name']);

        if ($params['compress']) {
            $objects = [];
            $objects["compression"] = $params['compresstype'];
            $pool->setProperties($objects);
        }
        // Adds missing ZFS filesystems to the OMV core
        OMVModuleZFSUtil::fixOMVMntEnt($context);

        // Apply changes immmediately
        \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
            'modules' => [ 'collectd', 'fstab', 'monit', 'quota' ],
            'force' => TRUE
        ], $context);
    }

    public function importPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.importpool');

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

    public function exportPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');

        $pool = new OMVModuleZFSZpool($params['name']);
        $pool->export();
    }

    public function getObjectTree($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = OMVModuleZFSUtil::getZFSFlatArray();
        $new = [];
        foreach ($objects as $a){
            $new[$a['parentid']][] = $a;
        }
        $tree['data'] = OMVModuleZFSUtil::createTree($new, $new['root']);
        return $tree;
    }

    public function listCompressionTypes($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = [];
        $objects[] = 'gzip';
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('gzip-%d', $i);
        }
        $objects[] = 'lz4';
        $objects[] = 'lzjb';
        $objects[] = 'zle';
        $objects[] = 'zstd';
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('zstd-%d', $i);
        }
        $objects[] = 'zstd-fast';
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('zstd-fast-%d', $i);
        }
        for ($i = 10; $i <= 100; $i += 10) {
            $objects[] = sprintf('zstd-fast-%d', $i);
        }
        $objects[] = 'zstd-fast-500';
        $objects[] = 'zstd-fast-1000';
        return ($objects);
    }

    public function listPools($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = OMVModuleZFSUtil::getZFSFlatArray();
        return $this->applyFilter($objects, $params['start'],
          $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function listPoolsBg($params, $context)
    {
        return $this->callMethodBg("listPools", $params, $context);
    }

    public function getAllSnapshots($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, "rpc.common.getlist");
        $objects = OMVModuleZFSUtil::getAllSnapshots();
        return $this->applyFilter($objects, $params['start'],
          $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function getAllSnapshotsBg($params, $context)
    {
        return $this->callMethodBg("getAllSnapshots", $params, $context);
    }


    public function addObject($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.addobject');

        if (preg_match('/[\s]+/', $params['name'])) {
            throw new OMVModuleZFSException("Spaces are not allowed in object names");
        }
        switch ($params['type']) {
            case "clone":
                $tmp = new OMVModuleZFSSnapshot($params['path']);
                $tmp->clonesnap($params['pool'] . '/'.  $params['clonename']);
                break;
            case "filesystem":
                $name = $params['path'] . "/" . $params['name'];
                $tmp = OMVModuleZFSFilesystem::create($name);
                if (strlen($params['mountpoint']) > 0) {
                    $properties = [
                        'mountpoint' => $params['mountpoint'],
                        'atime' => 'off',
                        'acltype' => 'posix',
                        'xattr' => 'sa'
                    ];
                    $tmp->setProperties($properties);
                }
                // $mntent_uuid = \OMV\Uuid::uuid4();
                // OMVModuleZFSUtil::setMntentProperty($mntent_uuid,$name);
                $this->dispatcher->notify(OMV_NOTIFY_MODIFY,
                  "org.openmediavault.storage.zfs.filesystem", $context);
                break;
            case "snapshot":
                $name = $params['path'] . "@" . $params['name'];
                OMVModuleZFSSnapshot::create($name);
                break;
            case "volume":
                $name = $params['path'] . "/" . $params['name'];
                $volsize = str_replace([' ','i'], '', $params['size']);
                OMVModuleZFSZvol::create($name, $volsize, null, boolval($params['thinvol']));
                break;
            default:
                throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
                break;
        }
        // Adds missing ZFS filesystems to the OMV core
        OMVModuleZFSUtil::fixOMVMntEnt($context);
        // Apply changes immmediately
        \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
            'modules' => [ 'collectd', 'fstab', 'monit', 'quota' ],
            'force' => TRUE
        ], $context);
    }

    public function addObjectBg($params, $context)
    {
        return $this->callMethodBg("addObject", $params, $context);
    }

    public function deleteObject($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.deleteobject');

        $name = $params['name'];
        $mp = $params['mp'];
        switch ($params['type']) {
            case "Filesystem":
                $tmp = new OMVModuleZFSFilesystem($name);
                if (OMVModuleZFSUtil::checkOMVShares($context,$tmp,$mp))
                    throw new OMVModuleZFSException("Before deleting this filesystem, You must delete shares referencing this filesystem.");
                OMVModuleZFSUtil::deleteOMVMntEnt($context,$tmp,$mp);
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
                var_dump($tmp);
                if (OMVModuleZFSUtil::checkOMVShares($context,$tmp,$mp))
                    throw new OMVModuleZFSException("Before deleting this filesystem, You must delete shares referencing this filesystem.");
                OMVModuleZFSUtil::deleteOMVMntEnt($context,$tmp,$mp);

                $devices = $tmp->getAllDevices([
                    "excludeStates" => [
                        OMVModuleZFSVdevState::STATE_SPARE_INUSE,
                        OMVModuleZFSVdevState::STATE_UNAVAIL,
                        OMVModuleZFSVdevState::STATE_SPLIT,
                    ]
                ]);

                OMVModuleZFSZpool::clearZFSLabel($devices);
                break;
            default:
                throw new OMVModuleZFSException("Illegal type provided: " . $params['type']);
                break;
        }
        // Adds missing ZFS filesystems to the OMV core
        OMVModuleZFSUtil::fixOMVMntEnt($context);

        // Apply changes immmediately
        \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
            'modules' => [ 'collectd', 'monit', 'quota' ],
            'force' => TRUE
        ], $context);
    }

    public function deleteObjectBg($params, $context)
    {
        return $this->callMethodBg("deleteObject", $params, $context);
    }

    public function getProperties($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        //$this->validateMethodParams($params, 'rpc.zfs.getproperties');

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
        return [ "properties" => $objects ];
    }

    public function setProperties($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.setproperties');

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
            if ($property['modified'] !== true) {
                continue;
            }
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
        if ($tmp instanceof OMVModuleZFSFilesystem) {
            $this->dispatcher->notify(OMV_NOTIFY_MODIFY,
              "org.openmediavault.storage.zfs.filesystem", $context);
            // Apply changes immmediately
            \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
                'modules' => [ 'collectd', 'monit', 'quota' ],
                'force' => TRUE
            ], $context);
        }
    }

    public function inherit($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.inherit');

        // Create the background process.
        return $this->execBgProc(function($bgStatusFilename, $bgOutputFilename)
            use ($params) {
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
                        throw new OMVModuleZFSException("Illegal type provided: " .
                            $params['type']);
                        break;
                }
                $tmp->inherit($params['property']);
            }
        );
    }

    public function getObjectDetails($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.objectdetails');

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
        return ($output);
    }

    public function expandPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.expandpool');

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
        $disks = $params['devices'];

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

    public function scrubPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $pool = new OMVModuleZFSZpool($params['name']);
        $process = $pool->scrub();
    }

    public function getCandidates($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
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

    public function getEmptyCandidates($params, $context) {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [ "role" => OMV_ROLE_ADMINISTRATOR ]);
        // Get a list of all potential usable devices.
        if (FALSE === ($devs = \OMV\System\Storage\StorageDevice::enumerateUnused()))
            throw new \OMV\Exception("Failed to get list of unused devices.");
        // Prepare the result list.
        $result = [];
        foreach ($devs as $devk => $devv) {
            // Get the object that implements the given storage device.
            $sd = \OMV\System\Storage\StorageDevice::getStorageDevice($devv);
            if (is_null($sd) || !$sd->exists())
                continue;
            // Skip read-only devices like CDROM.
            if (TRUE === $sd->isReadOnly())
                continue;
            // Skip devices that are connected via USB.
            if (TRUE === $sd->isUsb())
                continue;
            // Check if the device is referenced/used by a plugin.
            $db = \OMV\Config\Database::getInstance();
            if (TRUE === $db->exists("conf.service", [
              "operator" => "stringContains",
              "arg0" => "devicefile",
              "arg1" => $sd->getDeviceFile()
              ]))
                continue;
            // Does this device already contain a filesystem?
            if (FALSE !== \OMV\System\Filesystem\Filesystem::hasFileSystem(
              $sd->getDeviceFile()))
                continue;
            // The device is a potential candidate to be a member of a
            // zfs RAID.
            $result[] = [
                "devicefile" => $sd->getDeviceFile(),
                "size" => $sd->getSize(),
                "vendor" => $sd->getVendor(),
                "serialnumber" => $sd->getSerialNumber(),
                "description" => $sd->getDescription()
            ];
        }
        return $result;
    }

    public function rename($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.rename');

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
        }
        // Apply changes immmediately
        \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
            'modules' => [ 'collectd', 'monit', 'quota' ],
            'force' => TRUE
        ], $context);
    }

    /**
     * Get the general ZFS configuration settings.
     *
     * @param params The method params
     * @param context The context of the caller.
     * @return A JSON object containing the fields required
     *  to configure Kerberos.
     * @throw E_CONFIG_GET_OBJECT_FAILED
     */
    function getSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $object = $this->database->getAssoc($this->dataModelPath);
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
    function setSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.setsettings');
        $object = $this->database->getAssoc($this->dataModelPath);
        $object->setAssoc($params);
        $this->database->set($object);
        return $object;
    }

    public function rollbackSnapshot($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');

        $snap = new OMVModuleZFSSnapshot($params['name']);
        $snap->rollback();

        return $snap;
    }

    public function fixPackages($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        \OMV\Rpc\Rpc::call('Config', 'applyChanges', [
            'modules' => [ 'zfs' ],
            'force' => TRUE
        ], $context);
    }
}
