<?php
require_once('openmediavault/object.inc');
require_once('openmediavault/module.inc');
require_once("Vdev.php");
require_once("Snapshot.php");
require_once("Dataset.php");
require_once("Zvol.php");
require_once("VdevType.php");
require_once("Utils.php");
require_once("Exception.php");

/**
 * Class containing information about the pool
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 */
class OMVModuleZFSZpool extends OMVModuleAbstract {
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
     * @association OMVModuleZFSVdev to vdevs
     */
    private $vdevs;

    /**
     * List of spares
     *
     * @var    array $spare
     * @access private
     * @association OMVModuleZFSVdev to spare
     */
    private $spare;

    /**
     * List of log
     *
     * @var    array $log
     * @access private
     * @association OMVModuleZFSVdev to log
     */
    private $log;

    /**
     * List of cache
     *
     * @var    array $cache
     * @access private
     * @association OMVModuleZFSVdev to cache
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
     * @association OMVModuleZFSSnapshot to snapshot
     */
    private $snapshot;

    /**
     * Array of OMVModuleZFSDataset
     *
     * @var    Dataset $dataset
     * @access private
     * @association OMVModuleZFSDataset to dataset
     */
    private $dataset;

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

	public function __construct($vdev, $opts = "") {
		$create_pool = true;

		if (is_array($vdev)) {
			$cmd = $this->getCommandString($vdev);
			$name = $vdev[0]->getPool();
			$type = $vdev[0]->getType();
		} else if ($vdev instanceof OMVModuleZFSVdev) {
			$cmd = $this->getCommandString(array($vdev));
			$name = $vdev->getPool();
			$type = $vdev->getType();
		} else {
			// Assume we make an instance of an existing pool
			$create_pool = false;
		}

		$this->vdevs = array();
		$this->spare = null;
		$this->log = null;
		$this->cache = null;
		$this->features = array();
		if ($create_pool) {
			$cmd = "zpool create $opts\"$name\" $cmd 2>&1";

			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				throw new OMVModuleZFSException(implode("\n", $output));
			else {
				$this->name = $name;
				$this->type = $type;
				if (is_array($vdev))
					$this->vdevs = $vdev;
				else
					array_push ($this->vdevs, $vdev);
				$this->setSize();
				$this->mountPoint = $this->getAttribute("mountpoint");
			}
		} else {
			$this->assemblePool($vdev);
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
    public function addVdev(array $vdevs, $opts= "") {
		$cmd = "zpool add \"" . $this->name . "\" " . $opts . $this->getCommandString($vdevs) . " 2>&1";
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException(implode("\n", $output));
		else
			$this->vdevs = array_merge($this->vdevs, $vdevs);
		$this->setSize();
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

		$cmd = "zpool add \"" . $this->name . "\" cache " . $this->getCommandString($vdevs);
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
        if (! $disks)
			$disks = $this->cache;

		foreach ($disks as $disk)
			$dist_str .= "$disk ";

		$cmd = "zpool remove \"" . $this->name . "\" $dist_str";
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
		else {
			foreach ($disks as $disk)
				$this->cache = $this->removeDisk($this->cache, $disk);
		}
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
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				throw new OMVModuleZFSException($output);

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
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				throw new OMVModuleZFSException($output);
			else
				$this->log = array();
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
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);

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
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
		else {
			foreach ($disks as $disk)
				$this->spare = $this->removeDisk($this->spare, $disk);
		}
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

    /**
     * XXX
     *
     * @return int
     * @access public
     */
    public function getSize() {
        return $this->size;
    }

    /**
     *
     * @return int
     * @access private
     */
    private function setSize() {
        $used = OMVModuleZFSUtil::SizeTobytes($this->getAttribute("used")[0]);
        $avail = OMVModuleZFSUtil::SizeTobytes($this->getAvailable());
	$this->size = OMVModuleZFSUtil::bytesToSize($avail + $used);
    }

    /**
     *
     * @return int
     * @access public
     */
    public function getAvailable() {
        return $this->getAttribute("available")[0];
    }

    /**
     * XXX
     *
     * @return string
     * @access public
     */
    public function getMountPoint() {
        return $this->mountPoint;
    }

    /**
     * XXX
     *
     * @param  array $features
     * @return void
	 * @throws OMVModuleZFSException
     * @access public
     */
    public function setFeatures(array $features) {
		foreach ($features as $feature => $value) {
			$cmd = "zpool set $feature=$value \"" . $this->name . "\"";
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				throw new OMVModuleZFSException($output);
		}
		$this->features = $this->getAllAttributes();
    }

    /**
     * We only return array of features for which the user can
     * change in GUI.
     *
     * @return array of features
     * @access public
     */
    public function getFeatures($internal = true) {
		$attrs = array();
		$featureSet = array(
			'recordsize', /* default 131072. 512 <= n^2 <=  131072*/
			'checksum', /* on | off */
			'compression', /* off | lzjb | gzip | zle | lz4 */
			'atime', /* on | off */
			'aclmode', /* discard | groupmask | passthrough | restricted */
			'aclinherit', /* discard | noallow | restricted | passthrough | passthrough-x */
			'casesensitivity', /* sensitive | insensitive | mixed */
			'primarycache', /* all | none | metadata */
			'secondarycache', /* all | none | metadata */
			'logbias', /* latency | throughput */
			'dedup', /* on | off */
			'sync' /* standard | always | disabled */
		);
		if (count($this->features) < 1)
			$this->features = $this->getAllAttributes();
		if ($internal) {
			foreach ($this->features as $attr => $val) {
				if (in_array($attr, $featureSet))
					$attrs[$attr] = $val['value'];
			}
		} else {
			foreach ($this->features as $attr => $val) {
				if (in_array($attr, $featureSet))
					$attrs[$attr] = $val;
			}
		}

		return $attrs;
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
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
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
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
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
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
    }

    /**
     * XXX
     *
     * @return string
	 * @throws OMVModuleZFSException
     * @access public
     */
    public function status() {
        $cmd = "zpool status \"" . $this->name . "\"";
		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);
    }

	/**
	 * Get a single property value associated with the Dataset
	 *
	 * @param string $property Name of the property to fetch
	 * @return array The returned array with the property. The property is an associative array with
	 * two elements, <value> and <source>.
	 * @access public
	 */
	public function getProperty($property) {
		$attrs = $this->getFeatures(false);
		return $attrs["$property"];
	}

	/**
	 * Get an associative array of all properties associated with the Snapshot
	 *
	 * @return array $properties Each entry is an associative array with two elements
	 * <value> and <source>
	 * @access public
	 */
	public function getProperties() {
		$attrs = $this->getFeatures(false);
		return $attrs;
	}

	/**
	 * Sets a number of Dataset properties. If a property is already set it will be updated with the new value.
	 *
	 * @param  array $properties An associative array with properties to set
	 * @return void
	 * @access public
	 */
	public function setProperties($properties) {
		foreach ($properties as $newpropertyk => $newpropertyv) {
			$cmd = "zfs set " . $newpropertyk . "=" . $newpropertyv . " \"" . $this->name . "\" 2>&1";
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$attr = $this->getAttribute($newpropertyk);
			$this->features[$newpropertyk] = $attr;
		}
	}

	/**
	 * Destroy the Dataset.
	 *
	 * @return void
	 * @access public
	 */
	public function destroy() {
		$cmd = "zpool destroy \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Clears a previously set proporty and specifies that it should be
	 * inherited from it's parent.
	 *
	 * @param string $property Name of the property to inherit.
	 * @return void
	 * @access public
	 */
	public function inherit($property) {
		$cmd = "zfs inherit " . $property . " \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$attr = $this->getAttribute($newpropertyk);
		$this->features[$newpropertyk] = $attr;
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
			if (is_object($vdev) == false)
				throw new OMVMODULEZFSException("Not object of class OMVModuleZFSVdev");
			if (is_a($vdev, OMVModuleZFSVdev) == false)
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
	 * Get an attribute from pool
	 *
	 * @param string $attribute
	 * @return string value
	 */
	public function getAttribute($attribute) {
		$cmd = "zpool list -H -o $attribute \"{$this->name}\"";
		OMVUtil::exec($cmd, $output, $result);
		if ($result) {
			$cmd = "zfs list -H -o $attribute \"{$this->name}\"";
			OMVUtil::exec($cmd, $output, $result);
			if ($result)
				return null;
		}

		return $output;
	}

	/**
	 * Get all attributes from pool
	 * @return array of attributes
	 * @throws OMVModuleZFSException
	 */
	private function getAllAttributes() {
		$attrs = array();
		$cmd = "zfs get -H all \"{$this->name}\"";

		try {
			OMVUtil::exec($cmd, $output, $result);
		} catch (OMVModuleZFSException $e) {}
		if ($result)
			throw new OMVModuleZFSException($output);
		$output = implode("\n", $output);
		$res = preg_match_all("/{$this->name}\s+(\w+)\s+([\w\d\.]+)\s+(\w+).*/", $output, $matches, PREG_SET_ORDER);
		if ($res == false || $res == 0)
			throw new OMVModuleZFSException("Error return by zpool get all: $output");
		foreach ($matches as $match) {
			$attrs[$match[1]] = array('value' => $match[2], 'source' => $match[3]);
		}

		return $attrs;
	}

	/**
	 * Get all Dataset properties from commandline and update object properties attribute
	 *
	 * @return void
	 * @access private
	 */
	private function updateAllProperties() {
		$this->features = $this->getAllAttributes();
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

	/**
	 * Construct existing pool
	 *
	 * @param string $name
	 * @return void
	 * @throws OMVModuleZFSException
	 */
	private function assemblePool($name) {
		$cmd = "zpool status -v \"$name\"";
		$types = 'mirror|raidz1|raidz2|raidz3';
		$dev = null;
		$type = null;
		$log = false;
		$cache = false;
		$start = true;

		OMVUtil::exec($cmd, $output, $result);
		if ($result)
			throw new OMVModuleZFSException($output);

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
							} else if (preg_match("/^\s*([\w\d-a-z0-9\:\.\-]+)\s+/", $line, $match)) {
								if ($dev)
									$dev .= " $match[1]";
								else
									$dev = "$match[1]";
							}
							break;
						case 'logs':
							if (preg_match("/^\s*([\w\d-]+)\s+/", $line, $match)) {
								if ($dev)
									$dev .= " $match[1]";
								else
									$dev = "$match[1]";
							}
							break;
						case 'cache':
						case 'spares':
							if (preg_match("/^\s*([\w\d-]+)\s+/", $line, $match)) {
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
		$this->setSize();
		$this->mountPoint = $this->getAttribute("mountpoint");
	}

	/**
	 * Create pool config from parsed input
	 *
	 * @param string $part
	 * @param string $type
	 * @param string $dev
	 * @return void
	 * @throws OMVModuleZFSException
	 */
	private function output($part, $type, $dev) {
		$disks = split(" ", $dev);
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

}
?>
