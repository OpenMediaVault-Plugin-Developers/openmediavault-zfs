<?php
require_once("Vdev.php");
require_once("Filesystem.php");
require_once("Zvol.php");
require_once("VdevType.php");
require_once("Utils.php");
require_once("Exception.php");
require_once("Dataset.php");
require_once("ZpoolStatus.php");
/**
 * Class containing information about the pool
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 */
class OMVModuleZFSZpool extends OMVModuleZFSFilesystem {

    // Attributes

    /**
     * Pool's status instance
     *
     * @var     OMVModuleZFSZpoolStatus $status
     * @access  private
     */
    private $status;

    /**
     * List of Vdev
     *
     * @var    array $vdevs
     * @access private
     * @association OMVModuleZFSVdev to vdevs
     * @todo Get rid of this and use $status instead
     */
    private $vdevs;

    /**
     * List of spares
     *
     * @var    array $spare
     * @access private
     * @association OMVModuleZFSVdev to spare
     * @todo Get rid of this and use $status instead
     */
    private $spare;

    /**
     * List of log
     *
     * @var    array $log
     * @access private
     * @association OMVModuleZFSVdev to log
     * @todo Get rid of this and use $status instead
     */
    private $log;

    /**
     * List of cache
     *
     * @var    array $cache
     * @access private
     * @association OMVModuleZFSVdev to cache
     * @todo Get rid of this and use $status instead
     */
    private $cache;

    /**
     * Array of OMVModuleZFSZvol
     *
     * @var    Zvol $zvol
     * @access private
     * @association OMVModuleZFSZvol to zvol
     */
    private $zvol;

    // Operations
    /**
     * Constructor
     *
     * @param $vdev OMVModuleZFSVdev or array(OMVModuleZFSVdev)
     * @throws OMVModuleZFSException
     */

    public function __construct($name) {
        $this->vdevs = [];
        $this->spare = null;
        $this->log = null;
        $this->cache = null;
        $this->assemblePool($name);

        $this->status = $this->readStatus($name);
    }

    /**
     * Get array of Vdev
     *
     * @return array
     * @access public
     */
    public function getVdevs() {
        return $this->vdevs;
    }

    /**
     * Add Vdev to pool
     *
     * @param  array $vdev array of OMVModuleZFSVdev
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function addVdev(array $vdevs, $opts= "") {
        $cmd = "zpool add \"" . $this->name . "\" " . $opts . $this->getCommandString($vdevs) . " 2>&1";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        $this->vdevs = array_merge($this->vdevs, $vdevs);
        //$this->setSize();
    }

    /**
     * XXX
     *
     * @param  OMVModuleZFSVdev $vdev
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function removeVdev(OMVModuleZFSVdev $vdev) {
        throw new OMVModuleZFSException("Cannot remove vdevs from a pool");
    }

    /**
     * Returns all "real" devices used by the pool for storage and other purposes
     * (eg. cache). Returns "absolute paths" (/dev/...) to the devices or GUID
     * if a device is unavailable for some reason.
     *
     * @param   array $options
     *  Additional options for the getter defined as an associative array:
     *  - "excludeStates" (array)
     *      Vdev states to be excluded from the list
     * @return  array
     * @throws  OMVModuleZFSException
     * @access  public
     */
    public function getAllDevices($options = []) {
        // Sanitize options
        if (!array_key_exists("excludeStates", $options)) {
            $options["excludeStates"] = [];
        }

        return $this->status->getAllDevices($options);
    }

    /**
     * XXX
     *
     * @param  OMVModuleZFSVdev $cache
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function addCache(OMVModuleZFSVdev $cache) {
        if ($cache->getType() != OMVModuleZFSVdevType::OMVMODULEZFSPLAIN)
            throw new OMVModuleZFSException("Only a plain Vdev can be added as cache");

        $cmd = "zpool add \"" . $this->name . "\" cache " . $this->getCommandString($vdevs);
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        $disks = $cache->getDisks();
        foreach ($disks as $disk) {
            array_push ($this->cache, $disk);
        }
    }

    /**
     * XXX
     *
     * @param array $disks
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function removeCache(array $disks = null) {
        if (! $disks)
            $disks = $this->cache;

        foreach ($disks as $disk)
            $dist_str .= "$disk ";

        $cmd = "zpool remove \"" . $this->name . "\" $dist_str";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        foreach ($disks as $disk)
            $this->cache = $this->removeDisk($this->cache, $disk);
    }

    /**
     * XXX
     *
     * @return Cache
     * @access public
     */
    public function getCache() {
        return $this->cache;
    }

    /**
     * XXX
     *
     * @param  OMVModuleZFSVdev $log
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function addLog(OMVModuleZFSVdev $log) {
        if ($log->getType() == OMVModuleZFSVdevType::OMVMODULEZFSPLAIN ||
            $log->getType() == OMVModuleZFSVdevType::OMVMODULEZFSMIRROR) {
            $cmd = "zpool add \"" . $this->name . "\" log " . $this->getCommandString($vdevs);
            OMVModuleZFSUtil::exec($cmd,$output,$result);
            $this->log = $log;
        } else
            throw new OMVModuleZFSException("Only a plain Vdev or mirror Vdev can be added as log");
    }

    /**
     * XXX
     *
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function removeLog() {
        foreach ($this->log as $vdev) {
            if ($vdev->getType() == OMVModuleZFSVdevType::OMVMODULEZFSMIRROR) {
                $cmd = "zpool remove \"" . $this->name . "\" mirror-$i";
            } else {
                $disks = $vdev->getDisks();
                foreach ($disks as $disk)
                    $dist_str .= "$disk ";
                $cmd = "zpool remove \"" . $this->name . "\" $disk_str";
            }
            OMVModuleZFSUtil::exec($cmd,$output,$result);
            $this->log = [];
        }
    }

    /**
     * XXX
     *
     * @return Log
     * @access public
     */
    public function getLog() {
        return $this->log;
    }

    /**
     * XXX
     *
     * @param  OMVModuleZFSVdev $spares
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function addSpare(OMVModuleZFSVdev $spares) {
        if ($spares->getType() != OMVModuleZFSVdevType::OMVMODULEZFSPLAIN)
            throw new OMVModuleZFSException("Only a plain Vdev can be added as spares");

        $cmd = "zpool add \"" . $this->name . "\" spare " . $this->getCommandString($vdevs);
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        $disks = $spares->getDisks();
        foreach ($disks as $disk) {
            array_push ($this->spare, $disk);
        }
    }

    /**
     * XXX
     *
     * @param  array $disks
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function removeSpare(array $disks = null) {
        if (! $disks)
            $disks = $this->spare;

        foreach ($disks as $disk)
            $dist_str .= "$disk ";

        $cmd = "zpool remove \"" . $this->name . "\" $dist_str";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        foreach ($disks as $disk)
            $this->spare = $this->removeDisk($this->spare, $disk);
    }

    /**
     * XXX
     *
     * @return list<Disk>
     * @access public
     */
    public function getSpares() {
        return $this->spare;
    }

    public function updateAllProperties(){
        $featureSet = [
            'recordsize', /* default 131072. 512 <= n^2 <=  131072*/
            'checksum', /* on |      */
            'compression', /* off | lzjb | gzip | zle | lz4 */
            'atime', /* on | off */
            'acltype', /* noacl | posixacl */
            'aclmode', /* discard | groupmask | passthrough | restricted */
            'aclinherit', /* discard | noallow | restricted | passthrough | passthrough-x */
            'xattr', /* on | off | sa */
            'casesensitivity', /* sensitive | insensitive | mixed */
            'primarycache', /* all | none | metadata */
            'secondarycache', /* all | none | metadata */
            'logbias', /* latency | throughput */
            'dedup', /* on | off */
            'sync', /* standard | always | disabled */
            'mountpoint',
            'used',
            'available'
        ];
        parent::updateAllProperties();
        $attrs = [];
        foreach ($this->properties as $attr => $val) {
            $attrs[$attr] = $val;
        }
        $this->properties=$attrs;
    }

    /**
     * XXX
     *
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function export() {
        $cmd = "zpool export \"" . $this->name . "\"";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
    }

    /**
     * XXX
     *
     * @param  string $name
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function import($name = null) {
        if ($name)
            $cmd = "zpool import \"$name\"";
        else
            $cmd = "zpool import";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
    }

    /**
     * XXX
     *
     * @return void
     * @throws OMVModuleZFSException
     * @access public
     */
    public function scrub() {
        $cmd = "zpool scrub \"" . $this->name . "\"";
        return OMVModuleZFSUtil::exec($cmd,$output,$result);
    }

    /**
     * Returns the latest time a pool was scrubbed.
     *
     */
    public function getLatestScrub() {
        $cmd = "zpool status \"" . $this->name . "\" 2>&1";
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
     * Returns the status of a pool.
     *
     */
    public function getPoolStatus() {
        $cmd = "zpool status \"" . $this->name . "\" 2>&1";
        OMVModuleZFSUtil::exec($cmd,$out,$res);
        foreach ($out as $line) {
            if (preg_match('/errors: (.*)/', $line, $match)) {
                if (strcmp($match[1], "No known data errors") === 0)
                    return "OK";
                return "Error";
            }
        }
    }

    /**
     * Returns the state of a pool.
     *
     */
    public function getPoolState() {
        $cmd = "zpool get -H health \"" . $this->name . "\" 2>&1";
        OMVModuleZFSUtil::exec($cmd,$out,$res);
        $tmpary = preg_split('/\t+/', $out[0]);
        return $tmpary[2];
    }

    /**
     * Convert array of Vdev to command string
     *
     * @param array $vdevs
     * @return string
     * @throws OMVMODULEZFSException
     */
    public static function getCommandString(array $vdevs) {
        $adds = [];

        foreach ($vdevs as $vdev) {
            if (is_object($vdev) == false)
                throw new OMVMODULEZFSException("Not object of class OMVModuleZFSVdev");
            if (!($vdev instanceof OMVModuleZFSVdev))
                throw new OMVMODULEZFSException("Object is not of class OMVModuleZFSVdev");
            $type = $vdev->getType();
            $command = "";

            switch ($type) {
                case OMVModuleZFSVdevType::OMVMODULEZFSPLAIN: break;
                case OMVModuleZFSVdevType::OMVMODULEZFSMIRROR: $command = "mirror"; break;
                case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1: $command = "raidz1"; break;
                case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2: $command = "raidz2"; break;
                case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3: $command = "raidz3"; break;
                default:
                    throw new OMVMODULEZFSException("Unknown Vdev type");
            }
            $disks = $vdev->getDisks();
            $diskStr = "";
            foreach($disks as $disk) {
                $diskStr .= " $disk";
            }

            array_push ($adds, $command . $diskStr);
        }

        return implode(" ", $adds);
    }

    /**
     * Remove a disk from array
     *
     * @param array $array
     * @param string $disk
     * @return array
     */
    private function removeDisk(array $array, $disk) {
        $new_disks = [];

        foreach ($array as $item) {
            if (strcmp($item, $disk) != 0)
                array_push ($new_disks, $item);
        }

        return $new_disks;
    }

    /**
     * Read pool's status
     * and return it as a nested object
     *
     * @param string $poolName
     * @return array
     * @throws OMVModuleZFSException
     */
    private function readStatus($poolName) {
        // Get the pool's status,
        // and use -P flag to make sure that we have full paths to used vdevs.
        $cmd = "zpool status -P \"{$poolName}\" 2>&1";

        OMVModuleZFSUtil::exec($cmd, $cmdOutput, $exitCode);

        if ($exitCode !== 0) {
            throw new OMVMODULEZFSException("Could not read pool's status ({$poolName})");
        }

        return new OMVModuleZFSZpoolStatus($cmdOutput);
    }

    /**
     * Construct existing pool
     *
     * @param string $name
     * @return void
     * @throws OMVModuleZFSException
     * @todo Get rid of this and use OMVModuleZFSZpoolStatus instead
     */
    private function assemblePool($name) {
        $cmd = "zpool status -v \"$name\"";
        $types = 'mirror|raidz1|raidz2|raidz3';
        $dev = null;
        $type = null;
        $log = false;
        $cache = false;
        $start = true;

        OMVModuleZFSUtil::exec($cmd,$output,$result);

        $this->name = $name;
        foreach($output as $line) {
            if (! strstr($line, PHP_EOL))
                $line .= PHP_EOL;
            if ($start) {
                    if (preg_match("/^\s*NAME/", $line))
                            $start = false;
                    continue;
            } else {
                if (preg_match("/^\s*$/", $line)) {
                    if ($dev) {
                        $this->output($part, $type, $dev);
                    }
                    break;
                } else if (preg_match("/^\s*($name|logs|cache|spares)/", $line, $match)) {
                    if ($dev) {
                        $this->output($part, $type, $dev);
                        $dev = null;
                        $type = null;
                    }
                    $part = $match[1];
                } else {
                    switch ($part) {
                        case $name:
                            if (preg_match("/^\s*($types)/", $line, $match)) {
                                /* new vdev */
                                if ($type) {
                                    $this->output(null, $type, $dev);
                                    $dev = null;
                                }
                                $type = $match[1];
                            } else if (preg_match("/^\s*([\w\da-z0-9\:\.\-]+)\s+/", $line, $match)) {
                                if ($dev)
                                    $dev .= " $match[1]";
                                else
                                    $dev = "$match[1]";
                            }
                            break;
                        case 'logs':
                            if (preg_match("/^\s*([\w\d]+)\s+/", $line, $match)) {
                                if ($dev)
                                    $dev .= " $match[1]";
                                else
                                    $dev = "$match[1]";
                            }
                            break;
                        case 'cache':
                        case 'spares':
                            if (preg_match("/^\s*([\w\d]+)\s+/", $line, $match)) {
                                if ($dev)
                                    $dev .= " $match[1]";
                                else
                                    $dev = "$match[1]";
                            }
                            break;
                        default:
                            throw new Exception("$part: Unknown pool part");
                    }
                }
            }
        }
    }

    /**
     * Destroy the pool.
     *
     * @return void
     * @access public
     */
    public function destroy() {
        $cmd = "zpool destroy \"" . $this->name . "\" 2>&1";
        OMVModuleZFSUtil::exec($cmd,$out,$res);
    }


    public static function create($vdev,$opts=""){
        if (is_array($vdev)) {
            $cmd = OMVModuleZFSZpool::getCommandString($vdev);
            $name = $vdev[0]->getPool();
        } else if ($vdev instanceof OMVModuleZFSVdev) {
            $cmd = OMVModuleZFSZpool::getCommandString(array($vdev));
            $name = $vdev->getPool();
        }
        $cmd = "zpool create $opts \"$name\" $cmd 2>&1";
        OMVModuleZFSUtil::exec($cmd,$output,$result);
        $uuid = \OMV\Environment::get("OMV_CONFIGOBJECT_NEW_UUID");
        // $tmp->setMntentProperty($uuid);
        return new OMVModuleZFSZpool($name);
    }
    /**
     * Create pool config from parsed input
     *
     * @param string $part
     * @param string $type
     * @param string $dev
     * @return void
     * @throws OMVModuleZFSException
     * @todo Get rid of this and use OMVModuleZFSZpoolStatus instead
     */
    private function output($part, $type, $dev) {
        $disks = explode(" ", $dev);
        switch ($part) {
            case 'logs':
                if ($type && $type != 'mirror')
                    throw new Exception("$type: Logs can only be mirror or plain");
                if ($type)
                    $this->log = new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSMIRROR, $disks);
                else
                    $this->log = new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSPLAIN, $disks);
                break;
            case 'cache':
                if ($type)
                    throw new Exception("$type: cache can only be plain");
                $this->cache = new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSPLAIN, $disks);
                break;
            case 'spares':
                if ($type)
                    throw new Exception("$type: spares can only be plain");
                $this->spare = new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSPLAIN, $disks);
                break;
            default:
                if ($type) {
                    switch ($type) {
                        case 'mirror':
                            array_push($this->vdevs, new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSMIRROR, $disks));
                            $this->type = OMVModuleZFSVdevType::OMVMODULEZFSMIRROR;
                            break;
                        case 'raidz1':
                            array_push($this->vdevs, new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1, $disks));
                            $this->type = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1;
                            break;
                        case 'raidz2':
                            array_push($this->vdevs, new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2, $disks));
                            $this->type = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2;
                            break;
                        case 'raidz3':
                            array_push($this->vdevs, new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3, $disks));
                            $this->type = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3;
                            break;
                    }
                } else {
                    array_push($this->vdevs, new OMVModuleZFSVdev($this->name, OMVModuleZFSVdevType::OMVMODULEZFSPLAIN, $disks));
                    $this->type = OMVModuleZFSVdevType::OMVMODULEZFSPLAIN;
                }
            break;
        }
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



}
?>
