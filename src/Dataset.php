<?php
require_once("Exception.php");
require_once("openmediavault/util.inc");

/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSDataset {
    // Attributes
    /**
     * Name of Dataset
     *
     * @var    string $name
     * @access private
     */
    private $name;
    /**
     * Mountpoint of the Dataset
     *
     * @var    string $mountPoint
     * @access private
     */
    private $mountPoint;

    /**
     * Array with properties assigned to the Dataset
     *
     * @var    array $properties
     * @access private
     */
    private $properties;

	// Associations
	// Operations

	/**
	 * Constructor
	 *
	 * @param string $name Name of the new Dataset
	 * @param array $properties An associative array with properties to set when creating the Dataset
	 * @throws OMVModuleZFSException
	 *
	 */
	public function __construct($name, array $properties = null) {
		$cmd = "zfs create -p " . $name . " 2>&1";
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		$this->name = $name;
		$this->updateAllProperties();
		$this->setProperties($properties);
		$this->mountPoint = $this->properties["mountpoint"][0];
	}

	/**
	 * Return name of the Dataset
	 *
	 * @return string $name
	 * @access public
	 */
	public function getName() {
		return $this->name;
	}

	/**
	 * Get the mountpoint of the Dataset
	 *
	 * @return string $mountPoint
	 * @access public
	 */
	public function getMountPoint() {
		return $this->mountPoint;
	}

	/**
	 * Get a single property value associated with the Dataset
	 *
	 * @param string $property Name of the property to fetch
	 * @return array The returned array key 0=property value and key 1=property source.
	 * @access public
	 */
	public function getProperty($property) {
		return $this->properties["$property"];
	}

	/**
	 * Get an associative array of all properties associated with the Dataset.
	 *
	 * @return array $properties Each entry is an array where key 0=property value and key
	 * 1=property source.
	 *
	 * @access public
	 */
	public function getProperties() {
		return $this->properties;
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
			$cmd = "zfs set " . $newpropertyk . "=" . $newpropertyv . " " . $this->name;
			OMVUtil::exec($cmd,$out,$res);
			if ($res) {
				throw new OMVModuleZFSException(implode("\n", $out));
			}
			$this->updateProperty($newpropertyk);
		}
	}

	/**
	 * Get all Dataset properties from commandline and update object properties attribute
	 *
	 * @throws OMVModuleZFSException
	 * @access private
	 */ 
	private function updateAllProperties() {
		$cmd = "zfs get -H all " . $this->name;
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		unset($this->properties);
		foreach ($out as $line) {
			$tmpary = preg_split('/\t+/', $line);
			$this->properties["$tmpary[1]"] = array($tmpary[2], $tmpary[3]);
		}
	}

	/**
	 * Get single Datset property from commandline and update object property attribute
	 *
	 * @param string $property Name of the property to update
	 * @throws OMVModuleZFSException
	 * @access private
	 */
	private function updateProperty($property) {
		$cmd = "zfs get -H " . $property . " " . $this->name;
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		$tmpary = preg_split('/\t+/', $out[0]);
		$this->properties["$tmpary[1]"] = array($tmpary[2], $tmpary[3]);
	}

	/**
	 * Destroy the Dataset.
	 *
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public function destroy() {
		$cmd = "zfs destroy " . $this->name;
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
	}

	/**
	 * Renames a Dataset
	 *
	 * @param string $newname New name of the Dataset
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public function rename($newname) {
		$cmd = "zfs rename -p " . $this->name . " " . $newname . " 2>&1";
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		$this->name = $newname;
	}

}

?>
