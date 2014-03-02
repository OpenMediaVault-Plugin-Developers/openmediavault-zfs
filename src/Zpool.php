<?php
require_once("Vdev.php");
require_once("Snapshot.php");
require_once("Dataset.php");
require_once("Zvol.php");
require_once("Exception.php");

/**
 * Class containing information about the pool
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 */
class OMVModuleZFSZpool extends OMVModuleAbstract
		implements OMVNotifyListener {
    // Attributes
    /**
     * Name of pool
     *
     * @var    string $name
     * @access private
     */
    private $name;

    /**
     * List of Vdev
     *
     * @var    array $vdevs
     * @access private
     * @accociation OMVModuleZFSVdev to vdevs
     */
    private $vdevs;

    /**
     * List of spares
     *
     * @var    array $spare
     * @access private
     * @accociation OMVModuleZFSVdev to spare
     */
    private $spare;

    /**
     * List of log
     *
     * @var    array $log
     * @access private
     * @accociation OMVModuleZFSVdev to log
     */
    private $log;

    /**
     * List of cache
     *
     * @var    array $cache
     * @access private
     * @accociation OMVModuleZFSVdev to cache
     */
    private $cache;

    /**
     * Pool size
     *
     * @var    int $size
     * @access private
     */
    private $size;

    /**
     * Pool's mountpoint
     *
     * @var    string $mountPoint
     * @access private
     */
    private $mountPoint;

    /**
     * List of features
     *
     * @var    array $features
     * @access private
     */
    private $features;

    // Associations
    /**
     * Array of OMVModuleZFSSnapshot.
     *
     * @var    array $snapshot
     * @access private
     * @accociation OMVModuleZFSSnapshot to snapshot
     */
    private $snapshot;

    /**
     * Array of OMVModuleZFSDataset
     *
     * @var    Dataset $dataset
     * @access private
     * @accociation OMVModuleZFSDataset to dataset
     */
    private $dataset;

    /**
     * Array of OMVModuleZFSZvol
     *
     * @var    Zvol $zvol
     * @access private
     * @accociation OMVModuleZFSZvol to zvol
     */
    private $zvol;

    // Operations
	/**
	 * Constructor
	 *
	 * @param $pool pool this mirror belongs to
     * @throws OMVModuleZFSException
	 */

	public function __construct($vdev) {
		if (is_array($vdev)) {
			$cmd = $this->getCommandString($vdev);
			$name = $vdev[0]->getPool();
			$type = $vdev[0]->getType();
		}
		else {
			$cmd = $this->getCommandString(array($vdev));
			$name = $vdev->getPool();
			$type = $vdev->getType();
		}
		$cmd = "zpool create $name $cmd";

		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
		else {
			$this->vdevs = array();
			$this->spare = array();
			$this->log = array();
			$this->cache = array();
			$this->features = array();
			$this->name = $name;
			$this->type = $type;
			if (is_array($vdev))
				$this->vdevs = $vdev;
			else
				array_push ($this->vdevs, $vdev);
			$this->size = $this->getAttribute("size");
			$this->mountPoint = $this->getAttribute("mountpoint");
		}
	}

    /**
     * Get pool name
     *
     * @return string
     * @access public
     */
    public function getName() {
        return $this->name;
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
    public function addVdev(array $vdevs) {
		$cmd = "zpool add " . $this->getName() . " " . $this->getCommandString($vdevs);
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
		else
			$this->vdevs = array_merge($this->vdevs, $vdevs);
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

		$cmd = "zpool add " . $this->getName() . " cache " . $this->getCommandString($vdevs);
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);

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
		$errors = array();
		$exception = null;

        if (! $disks)
			$disks = $this->cache;

		foreach ($disks as $disk) {
			$cmd = "zpool remove " . $this->getName() . " $disk";
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				array_push ($errors, $output);
			else
				$this->cache = $this->removeDisk($this->cache, $disk);
		}

		foreach ($errors as $error) {
			if ($exception)
				$exception .= "\n$error";
			else
				$exception = $error;
		}

		if ($exception)
			throw new OMVModuleZFSException($exception);
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
     * @access public
     */
    public function addLog(OMVModuleZFSVdev $log) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void
     * @access public
     */
    public function removeLog() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return Log
     * @access public
     */
    public function getLog() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  array $spares
     * @return void
     * @access public
     */
    public function addSpare(array $spares) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Disk $spare
     * @return void
     * @access public
     */
    public function removeSpare($spare) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return list<Disk>
     * @access public
     */
    public function getSpares() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return int
     * @access public
     */
    public function getSize() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return string
     * @access public
     */
    public function getMountPoint() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  array $features
     * @return void
     * @access public
     */
    public function setFeatures(array $features) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return list<Feature>
     * @access public
     */
    public function getFeatures() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void
     * @access public
     */
    public function export() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  string $name
     * @return void
     * @access public
     */
    public function import($name) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void
     * @access public
     */
    public function scrub() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return string
     * @access public
     */
    public function status() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    public function bindListeners(OMVNotifyDispatcher $dispatcher) {
        $dispatcher->addListener(
          OMV_NOTIFY_EVENT,
          "org.openmediavault.module.service.nfs.start",
          array($this, "onNotify"));
        $dispatcher->addListener(
          OMV_NOTIFY_EVENT,
          "org.openmediavault.module.service.nfs.stop",
          array($this, "onNotify"));
        $dispatcher->addListener(
          OMV_NOTIFY_EVENT,
          "org.openmediavault.module.service.nfs.applyconfig",
          array($this, "onNotify"));
    }

	/**
	 * XXX
	 * org.openmediavault.module.service.<servicename>.start
	 * org.openmediavault.module.service.<servicename>.stop
	 * org.openmediavault.module.service.<servicename>.applyconfig
	 *
	 * @param string event
	 * @access public
	 */
	public function onNotify($event) {
        trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * Convert array of Vdev to command string
	 *
	 * @param array $vdevs
	 * @return string
	 * @throws OMVMODULEZFSException
	 */
	private function getCommandString(array $vdevs) {
		$adds = array();

		foreach ($vdevs as $vdev) {
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

		return join(" ", $adds);
	}

	/**
	 * Get an attribute from pool
	 *
	 * @param string $attribute
	 * @return string value
	 */
	private function getAttribute($attribute) {
		$cmd = "zpool list -H -o $attribute {$this->name}";
		OMVUtil::exec($cmd, $output, $result);
		if ($result) {
			$cmd = "zfs list -H -o $attribute {$this->name}";
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				return null;
		}

		return $output;
	}

	/**
	 * Remove a disk from array
	 *
	 * @param array $array
	 * @param string $disk
	 * @return array
	 */
	private function removeDisk(array $array, $disk) {
		$new_disks = array();

		foreach ($array as $item) {
			if (strcmp($item, $disk) != 0)
				array_push ($new_disks, $item);
		}

		return $new_disks;
	}
}

?>
