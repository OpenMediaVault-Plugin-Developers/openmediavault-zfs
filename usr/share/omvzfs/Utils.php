<?php
require_once("Exception.php");
require_once("Filesystem.php");
require_once("Zvol.php");
require_once("Vdev.php");
require_once("Zpool.php");
use OMV\System\Process;
use OMV\Rpc\Rpc;
use OMV\Uuid;


/**
 * Helper class for ZFS module
 */
class OMVModuleZFSUtil {

    /**
     * Regex matching the "/dev/DEVICENAME" strings.
     * DEVICENAME can be retrieved from the first capture group.
     *
     * @var  REGEX_DEVBYID_DEVNAME
     * @access public
     */
	const REGEX_DEVBYID_DEVNAME = "/^.*\/([A-Za-z0-9]+.*)$/";

    /**
     * Get the /dev/sdX device name from /dev/disk/by-uuid
     *
     */
    public static function getDevByUuid($uuid) {
        $cmd = "ls -la /dev/disk/by-uuid/" . $uuid;
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if (count($out) === 1) {
            if (preg_match('/^.*\/([a-z0-9]+)$/', $out[0], $match)) {
                $disk = $match[1];
                return($disk);
            }
        }
        throw new OMVModuleZFSException("Unable to find /dev/disk/by-uuid/" . $uuid);
    }

    /**
     * Get the /dev/DEVNAME device name from /dev/disk/by-id
     *
     */
    public static function getDevByID($id) {
        $cmd = "ls -la /dev/disk/by-id/" . $id;
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if (count($out) === 1) {
            if (preg_match(OMVModuleZFSUtil::REGEX_DEVBYID_DEVNAME, $out[0], $match)) {
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
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if (count($out) === 1) {
            if (preg_match('/^.*\/([a-z0-9]+.*)$/', $out[0], $match)) {
                $disk = $match[1];
                return($disk);
            }
        }
        throw new OMVModuleZFSException("Unable to find /dev/disk/by-path/" . $path);
    }

    public static function deleteOMVMntEnt($context, $filesystem, $mp) {
        $object = Rpc::call("FsTab", "getByDir", ["dir" => $mp], $context);
        $filesystem->updateProperty("mountpoint");
        if ($object and $object['type'] == 'zfs' and $object['dir'] == $mp) {
            Rpc::call("FsTab", "delete", ["uuid" => $object['uuid']], $context);
            Rpc::call("Config", "applyChanges", ["modules" => [ "fstab" ], "force" => TRUE], $context);
            if ($filesystem->exists()) {
                $filesystem->destroy();
            }
        } else {
            throw new OMVModuleZFSException("No such Mntent exists");
        }

    }

    /**
     * Deletes all shared folders pointing to the specifc path
     *
     */
    public static function checkOMVShares($context, $filesystem, $mp) {
        var_dump($mp);
        $object = Rpc::call("FsTab", "getByDir", ["dir" => $mp], $context);
        $uuid = $object['uuid'];
        $shares = Rpc::call("ShareMgmt", "enumerateSharedFolders", [], $context);
        $objects = [];
        foreach ($shares as $share) {
            if ($share['mntentref'] == $uuid) {
                $objects[] = $share;
            }
        }
        if ($objects) {
            return TRUE;
        } else {
            return FALSE;
        }
    }

    /**
     * Get /dev/disk/by-path from /dev/sdX
     *
     * @return string Disk identifier
     */
    public static function getDiskPath($disk) {
        preg_match("/^\/dev\/(.*)$/", $disk, $identifier);
        $cmd = "ls -la /dev/disk/by-path | grep '" . preg_quote($identifier[1]) . "$'";
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if (is_array($out)) {
            $cols = preg_split('/[\s]+/', $out[0]);
            return($cols[count($cols) - 3]);
        }
    }

    /**
     * Get /dev/disk/by-id from /dev/DEVNAME
     *
     * @return string Disk identifier
     */
    public static function getDiskId($disk) {
        preg_match(OMVModuleZFSUtil::REGEX_DEVBYID_DEVNAME, $disk, $identifier);
        $cmd = "ls -la /dev/disk/by-id | grep '" . preg_quote($identifier[1]) . "$'";
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if (is_array($out)) {
            $cols = preg_split('/[\s]+/', $out[0]);
            return($cols[count($cols) - 3]);
        }
    }

    public static function renameOMVMntEnt($context, $filesystem) {
        $db = \OMV\Config\Database::getInstance();
        $filesystem->updateProperty("mountpoint");
        $object = Rpc::call("FsTab", "getByDir", ["dir" => $filesystem->getMountPoint()], $context);
        $object['fsname'] = $filesystem->getName();
        $config = $db->get("conf.system.filesystem.mountpoint", $object['uuid']);
        $db->set($config, TRUE);
    }

    public static function relocateOMVMntEnt($context, $filesystem) {
        $db = \OMV\Config\Database::getInstance();
        $children = $filesystem->getChildren();
        $children[] = $filesystem;
        foreach ($children as $child) {
            $object = Rpc::call("FsTab", "getByFsName", ["fsname" => $child->getName()], $context);
            $child->updateProperty("mountpoint");
            $object['dir'] = $child->getMountPoint();
            $config = $db->get("conf.system.filesystem.mountpoint", $object['uuid']);
            $db->set($config, TRUE);
        }
        //REMOUNT
    }

    /**
     * Add any missing ZFS filesystems to the OMV backend
     *
     */
    public static function fixOMVMntEnt($context) {
        $src = '/etc/openmediavault/config.xml';
        $dir = '/etc/openmediavault/';
        $timestamp = date('Ymd-His');
        $dst = "$dir/config-zfs-$timestamp.xml";

        if (!@copy($src, $dst)) {
            throw new \OMV\Exception("Failed to copy $src to $dst");
        }

        /* Keep only the 20 newest backup files */
        $files = glob("$dir/config-*.xml", GLOB_NOSORT);
        if ($files === false) {
            throw new \OMV\Exception("Failed to list backup files in $dir");
        }

        /* Sort newest → oldest using filemtime */
        usort($files, function($a, $b) {
            return filemtime($b) <=> filemtime($a);  // descending
        });

        /* Remove any beyond the first 20 */
        $keep = 20;
        $toDelete = array_slice($files, $keep);

        foreach ($toDelete as $file) {
            @unlink($file);
        }

        // 1. Build list of current ZFS datasets from `zfs list`
        $current = [];
        try {
            $out = [];
            $res = 0;
            OMVModuleZFSUtil::exec('zfs list -H -o name,mountpoint', $out, $res);
            if ($res === 0) {
                foreach ($out as $line) {
                    $line = trim($line);
                    if ($line === "") {
                        continue;
                    }
                    $parts = preg_split('/\s+/', $line, 2);
                    if (count($parts) !== 2) {
                        continue;
                    }
                    list($name, $mntpoint) = $parts;
                    // Skip non-mountable datasets
                    if ($mntpoint === "-" || $mntpoint === "none" || $mntpoint === "legacy") {
                        continue;
                    }
                    // Match original behavior: skip root "/"
                    if ($mntpoint === "/") {
                        continue;
                    }
                    $current[] = [
                        "fsname" => $name,
                        "dir"    => $mntpoint
                    ];
                }
            }
        } catch (\Exception $e) {
            // If this fails, bail out – we can't safely sync anything
            return;
        }

        // 2. Read existing ZFS FsTab entries
        $prev = \OMV\Rpc\Rpc::call("FsTab", "enumerateEntries", [], $context);
        $prev = array_filter($prev, function ($element) {
            return isset($element["type"]) && $element["type"] === "zfs";
        });

        // 3. Compute which entries are missing (ADD ONLY)
        $compare = function ($a, $b) {
            $fsCmp = strcmp($a["fsname"], $b["fsname"]);
            if ($fsCmp !== 0) {
                return $fsCmp;
            }
            return strcmp($a["dir"], $b["dir"]);
        };

        // $add = entries present in $current but not in $prev
        $add = array_udiff($current, $prev, $compare);

        // 4. Add new mntent entries for missing datasets
        foreach ($add as $object) {
            $fsname = $object["fsname"];
            $dir    = $object["dir"];

            // IMPORTANT:
            // For NEW mountpoints we MUST use OMV_CONFIGOBJECT_NEW_UUID.
            // Passing any other unknown UUID to FsTab::set will cause a
            // Database->get() XPath error if that UUID doesn't exist.
            $uuid = \OMV\Environment::get("OMV_CONFIGOBJECT_NEW_UUID");

            // Fetch mount options via findmnt (best-effort)
            $opts = "";
            try {
                $out = [];
                $res = 0;
                $cmd = sprintf(
                    "findmnt -no OPTIONS --target %s 2>/dev/null",
                    escapeshellarg($dir)
                );
                OMVModuleZFSUtil::exec($cmd, $out, $res);
                if ($res === 0) {
                    $opts = trim(implode("", $out));
                }
            } catch (\Exception $e) {
                $opts = "";
            }

            $entry = [
                "uuid"   => $uuid,
                "fsname" => $fsname,
                "dir"    => $dir,
                "type"   => "zfs",
                "opts"   => $opts,
                "freq"   => 0,
                "passno" => 0
            ];

            // FsTab::set will:
            //  - create a new conf.system.filesystem.mountpoint object
            //  - assign a real UUID, replacing OMV_CONFIGOBJECT_NEW_UUID internally.
            \OMV\Rpc\Rpc::call("FsTab", "set", $entry, $context);

            // Do NOT update omvzfsplugin:uuid here.
            // We want the dataset property to still hold the *old* value (if any)
            // so we can map old_uuid -> new_uuid when fixing SharedFolders below.
        }

        // 5. Refresh ZFS mntents and fix SharedFolders UUID mapping
        $zfs_mntent = \OMV\Rpc\Rpc::call("FsTab", "enumerateEntries", [], $context);
        $zfs_mntent = array_filter($zfs_mntent, function ($element) {
            return isset($element["type"]) && $element["type"] === "zfs";
        });

        $sf_objects = \OMV\Rpc\Rpc::Call("ShareMgmt", "enumerateSharedFolders", [], $context);

        foreach ($zfs_mntent as $dataset) {
            $fsname = $dataset["fsname"];

            // STRICT pool name extraction: first component before '/'
            $pool = strstr($fsname, "/", true);
            if ($pool === false) {
                $pool = $fsname;
            }

            // Skip if pool not imported to avoid ZFS errors
            if (!OMVModuleZFSUtil::isPoolImported($pool)) {
                continue;
            }

            // Load ZFS filesystem object and its properties
            $tmp = new OMVModuleZFSFilesystem($fsname);
            $tmp->updateAllProperties();

            // old_uuid comes from ZFS dataset property "omvzfsplugin:uuid"
            // This may be:
            //  - equal to an existing mntent UUID (normal case)
            //  - an old UUID whose mntent was manually deleted
            //  - empty / missing (never imported before)
            $prop = $tmp->getProperty("omvzfsplugin:uuid");
            $old_uuid = (is_array($prop) && isset($prop["value"]) && $prop["value"] !== "")
                ? $prop["value"]
                : null;

            $new_uuid = $dataset["uuid"];
            $mountpoint = $tmp->getMountPoint();

            if ($old_uuid && $old_uuid !== $new_uuid) {
                // Only update shared folders whose path matches this dataset's mountpoint exactly
                $matching_sfs = array_filter($sf_objects, function ($sf) use ($mountpoint) {
                    if (!isset($sf["mntent"]["dir"])) {
                        return false;
                    }
                    $base = rtrim($sf["mntent"]["dir"], "/");
                    if (isset($sf["reldirpath"]) && $sf["reldirpath"] !== "") {
                        $path = rtrim($base . "/" . $sf["reldirpath"], "/");
                    } else {
                        $path = $base;
                    }
                    return $path === $mountpoint;
                });

                if (!empty($matching_sfs)) {
                    // This should update sharedfolder.mntentref from old_uuid to new_uuid
                    OMVModuleZFSUtil::fixOMVSharedFolders(
                        $old_uuid,
                        $new_uuid,
                        $matching_sfs,
                        $context
                    );
                }
            }

            // Finally, sync dataset property to the current mntent UUID so that:
            //  - Future runs see this as "current"
            //  - If the mntent is manually deleted, we can still recover state
            OMVModuleZFSUtil::setMntentProperty($new_uuid, $fsname);
        }
    }

    /**
     * Get an array with all ZFS objects
     *
     * @return An array with all ZFS objects
     */
    public static function getZFSFlatArray() {
        $prefix = "root/pool-";
        $objects = [];
        $cmd = "zfs list -p -H -t all -o name,type 2>&1";
        $expanded = true;
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            $path = $parts[0];
            $type = $parts[1];
            $subdirs = preg_split('/\//', $path);
            $root = $subdirs[0];
            $tmp = [];

            switch ($type) {
            case "filesystem":
                if (strpos($path, '/') === false) {
                    // This is a Pool
                    $tmp = array(
                        'id' => $prefix . $path,
                        'parentid' => 'root',
                        'name' => $path,
                        'type' => 'Pool',
                        'icon' => 'images/raid.png',
                        'expanded' => $expanded,
                        'path' => $path
                    );
                    $pool = new OMVModuleZFSZpool($path);
                    $pool->updateAllProperties();
                    $tmp['origin'] = "n/a";
                    $tmp['size'] = $pool->getSize();
                    $tmp['used'] = $pool->getUsed();
                    $tmp['usedpercent'] = $tmp['used'] / $tmp['size'] * 100;
                    $tmp['available'] = $pool->getAvailable();
                    $tmp['mountpoint'] = $pool->getMountPoint();
                    $tmp['lastscrub'] = $pool->getLatestScrub();
                    $tmp['state'] = $pool->getPoolState();
                    $tmp['status'] = $pool->getPoolStatus();
                    array_push($objects, $tmp);
                } else {
                    // This is a Filesystem
                    preg_match('/(.*)\/(.*)$/', $path, $result);
                    $tmp = array(
                        'id' => $prefix . $path,
                        'parentid' => $prefix . $result[1],
                        'name' => $result[2],
                        'icon' => "images/filesystem.png",
                        'path' => $path,
                        'expanded' => $expanded
                    );
                    $ds =  new OMVModuleZFSFilesystem($path);
                    $ds->updateAllProperties();
                    // $props = $ds->getProperties();
                    // print_r($props);
                    if ($ds->isClone()) {
                        // This is a cloned Filesystem
                        $tmp['origin'] = $ds->getOrigin();
                    } else {
                        // This is a standard Filesystem.
                        $tmp['origin'] = "n/a";
                    }
                    $tmp['type'] = ucfirst($type);
                    $tmp['size'] = $ds->getSize();
                    $tmp['used'] = $ds->getUsed();
                    $tmp['usedpercent'] = $tmp['used'] / $tmp['size'] * 100;
                    $tmp['available'] = $ds->getAvailable();
                    $tmp['mountpoint'] = $ds->getMountPoint();
                    $tmp['lastscrub'] = "n/a";
                    $tmp['state'] = "n/a";
                    $tmp['status'] = "n/a";
                    array_push($objects, $tmp);
                }
                break;

            case "volume":
                preg_match('/(.*)\/(.*)$/', $path, $result);
                $tmp = array(
                    'id' => $prefix . $path,
                    'parentid' => $prefix . $result[1],
                    'name' => $result[2],
                    'type' => ucfirst($type),
                    'path' => $path,
                    'expanded' => $expanded
                );
                $vol = new OMVModuleZFSZvol($path);
                $vol->updateAllProperties();
                if ($vol->isClone()) {
                    // This is a cloned Volume
                    $tmp['origin'] = $vol->getOrigin();
                } else {
                    // This is a standard Volume
                    $tmp['origin'] = "n/a";
                }
                $tmp['type'] = ucfirst($type);
                $tmp['size'] = $vol->getSize();
                $tmp['used'] = $vol->getUsed();
                $tmp['usedpercent'] = $tmp['used'] / $tmp['size'] * 100;
                $tmp['available'] = $vol->getAvailable();
                $tmp['mountpoint'] = "n/a";
                $tmp['lastscrub'] = "n/a";
                if (!($vol->isThinVol())) {
                    $tmp['icon'] = "images/save.png";
                } else {
                    $tmp['icon'] = "images/zfs_thinvol.png";
                }
                $tmp['state'] = "n/a";
                $tmp['status'] = "n/a";
                array_push($objects, $tmp);
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
    public static function createTree(&$list, $parent) {
        $tree = [];
        foreach ($parent as $k => $l) {
            if (isset($list[$l['id']])) {
                $l['leaf'] = false;
                $l['data'] = OMVModuleZFSUtil::createTree($list, $list[$l['id']]);
            } else {
                $l['leaf'] = true;
            }
            $tree[] = $l;
        }
        return $tree;
    }

    /**
     * Get an array with all ZFS objects
     *
     * @return An array with all ZFS objects
     */
    public static function getAllSnapshots() {
        $prefix = "root/pool-";
        $objects = [];
        $cmd = "zfs list -p -H -t snapshot -o name,used,refer 2>&1";

        OMVModuleZFSUtil::exec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            $path = $parts[0];
            $used = $parts[1];
            $refer = $parts[2];
            $subdirs = preg_split('/\//', $path);
            $root = $subdirs[0];
            $tmp = [];

            preg_match('/(.*)\@(.*)$/', $path, $result);
            $subdirs = preg_split('/\//', $result[1]);
            $root = $subdirs[0];
            $tmp = array(
                'id' => $prefix . $path,
                'parent' => $result[1],
                'name' => $result[2],
                'type' => "Snapshot",
                'icon' => 'images/zfs_snap.png',
                'path' => $path
            );
            $tmp['used'] = $used;
            $tmp['refer'] = $refer;
            array_push($objects, $tmp);
        }
        return $objects;
    }

    /**
     * Helper function to execute a command and throw an exception on error
     * (requires stderr redirected to stdout for proper exception message).
     *
     * @param string $cmd Command to execute
     * @param array &$out If provided will contain output in an array
     * @param int &$res If provided will contain Exit status of the command
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public static function exec($cmd, &$out = null, &$res = null) {
        if (file_exists("/var/log/zfs_commands.log"))
            file_put_contents("/var/log/zfs_commands.log", $cmd . PHP_EOL, FILE_APPEND);

        $process = new Process($cmd);
        $process->execute($out, $res);
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
            return $bytes . ' B';

        } elseif (($bytes >= $kilobyte) && ($bytes < $megabyte)) {
            return round($bytes / $kilobyte, $precision) . ' KiB';

        } elseif (($bytes >= $megabyte) && ($bytes < $gigabyte)) {
            return round($bytes / $megabyte, $precision) . ' MiB';

        } elseif (($bytes >= $gigabyte) && ($bytes < $terabyte)) {
            return round($bytes / $gigabyte, $precision) . ' GiB';

        } elseif ($bytes >= $terabyte) {
            return round($bytes / $terabyte, $precision) . ' TiB';
        } else {
            return $bytes . ' B';
        }
    }

    /**
     * Convert human readable format to bytes
     *
     * @param string $humanformat Size in human readble format
     * @return integer
     */
    public static function SizeTobytes($humanformat) {
        $units = array("B", "K", "M", "G", "T");
        $kilobyte = 1024;
        $megabyte = $kilobyte * 1024;
        $gigabyte = $megabyte * 1024;
        $terabyte = $gigabyte * 1024;

        if ($humanformat === "0") {
            return 0;
        }

        $unit = substr($humanformat, -1);
        $num = substr($humanformat, 0, strlen($humanformat) - 1);

        if (is_numeric($unit)) {
            return int($humanformat);
        }
        if (in_array($unit, $units)) {
            switch ($unit) {
                case "B":
                    break;
                case "K":
                    $num *= $kilobyte;
                    break;
                case "M":
                    $num *= $megabyte;
                    break;
                case "G":
                    $num *= $gigabyte;
                    break;
                case "T":
                    $num *= $terabyte;
                    break;
            }
        } else {
            throw new OMVModuleZFSException("Unknown size unit");
        }

        return $num;
    }

    public static function isReferenced($mntent) {
        $mntent = $db->get("conf.system.filesystem.mountpoint", $mntent['uuid']);
        if ($db->isReferenced($mntent))
            return true;
        else
            return false;
    }

    /**
     * Sets a custom property for storing mntent uuid.
     *
     * @param  string internal database uuid of the mntent entry.
     * @return void
     * @access public
     */
    public static function setMntentProperty(string $mntent_uuid, string $fsname): void {
        $pool = self::getPoolFromFsname($fsname);
        if (!OMVModuleZFSUtil::isPoolImported($pool)) {
            return;
        }
        $prop = 'omvzfsplugin:uuid';
        $cmd  = sprintf(
            'zfs set %s=%s %s 2>&1',
            escapeshellarg($prop),
            escapeshellarg($mntent_uuid),
            escapeshellarg($fsname)
        );
        try {
            OMVModuleZFSUtil::exec($cmd, $out, $res);
            // Optional verification
            if ($res === 0) {
                $verifyCmd = sprintf(
                    'zfs get -H -o value %s %s 2>&1',
                    escapeshellarg($prop),
                    escapeshellarg($fsname)
                );
                $vout = []; $vres = 0;
                OMVModuleZFSUtil::exec($verifyCmd, $vout, $vres);
            }
        } catch (\Throwable $t) {
            // Non-fatal
        }
    }


    /**
     * Fix all shared folders that have a reference to the old uuid stored in the dataset property
     *
     * @param  string uuid stored in the dataset property
     * @return void
     * @access public
     */
    public static function fixOMVSharedFolders($old_uuid, $new_uuid, $sf_objects, $context) {
        $zfs_sfs = array_filter($sf_objects, function ($var) use ($old_uuid) {
            return ($var['mntentref'] == $old_uuid);
        });
        foreach ($zfs_sfs as $zfs_sf) {
            unset($zfs_sf['mntent'], $zfs_sf['_used'], $zfs_sf['privileges']);
            $zfs_sf['mntentref'] = $new_uuid;
            \OMV\Rpc\Rpc::Call("ShareMgmt", "set", $zfs_sf, $context);
        }
    }

    /**
     * Check if the pool is imported
     *
     * @param string $name Pool name
     * @return boolean
     * @throws OMVModuleZFSException
     */
    public static function isPoolImported($name) {
        $cmd = "zpool status " . $name . " &>/dev/null";
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if ($res === 0) {
            return TRUE;
        } else {
            return FALSE;
        }
    }


    public static function getPoolFromFsname(string $fsname): string {
        $pos = strpos($fsname, '/');
        return ($pos === false) ? $fsname : substr($fsname, 0, $pos);
    }

    public static function getStoredMntentUuid(string $fsname): ?string {
        $pool = self::getPoolFromFsname($fsname);
        if (!OMVModuleZFSUtil::isPoolImported($pool)) {
            return null;
        }
        $prop = 'omvzfsplugin:uuid';
        $cmd  = sprintf(
            'zfs get -H -o value %s %s 2>&1',
            escapeshellarg($prop),
            escapeshellarg($fsname)
        );
        try {
            $out = []; $res = 0;
            OMVModuleZFSUtil::exec($cmd, $out, $res);
            if ($res === 0) {
                $val = trim(implode("", $out));
                return ($val !== '-' && $val !== '') ? $val : null;
            }
        } catch (\Throwable $t) {
            // ignore
        }
        return null;
    }
}

?>
