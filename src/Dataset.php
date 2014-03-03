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
     * List of properties assigned to the Dataset
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
	 * @param array $properties An array of properties (strings) in the form <key>=<value> to set when creating the Dataset
	 * @throws OMVModuleZFSException
	 *
	 */
	public function __construct($name, array $properties = null) {
		$cmd = "zfs create ";
		if (isset($properties)) {
			foreach ($properties as $property) {
				$cmd .= "-o " . $property . " ";
			}
		}
		$cmd .= $name . " 2>&1";
		OMVUtil::exec($cmd,$out,$res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		unset($res);
		$this->name = $name;
		if (isset($properties)) {
			$this->properties = $properties;
			foreach ($properties as $property) {
				if (preg_match('/^mountpoint\=(.*)$/', $property, $res)) {
					$this->mountPoint = $res[1];
					continue;
				}
			}
		} else {
			$this->properties = array();
			$this->mountPoint = "/" . $name;
		}
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
	 * Get an array of properties associated with the Dataset
	 *
	 * @return array $properties
	 * @access public
	 */
	public function getProperties() {
		return $this->properties;
	}

	/**
	 * Sets a number of Dataset properties. If a property is already set it will be updated with the new value.
	 *
	 * @param  array $properties An array of strings in format <key>=<value>
	 * @return void
	 * @access public
	 */
	public function setProperties($properties) {
		foreach ($properties as $newproperty) {
			$cmd = "zfs set " . $newproperty . " " . $this->name;
			OMVUtil::exec($cmd,$out,$res);
			if ($res) {
				throw new OMVModuleZFSException(implode("\n", $out));
			}
			$tmp = explode("=", $newproperty);
			$newpropertyk = $tmp[0];
			$found = false;
			for ($i=0; $i<count($this->properties); $i++) {
				$tmp = explode("=", $this->properties[$i]);
				$oldpropertyk = $tmp[0];
				if (strcmp($newpropertyk, $oldpropertyk) == 0) {
					$this->properties[$i] = $newproperty;
					$found = true;
					continue;
				}
			}
			if (!$found) {
				array_push($this->properties, $newproperty);
			}
		}
	}

	/**
	 * Destroy the Dataset.
	 *
	 */
	public function destroy() {
		$cmd = "zfs destroy " . $this->name;
		OMVUtil::exec($cmd,$out,$res);
		if ($res == 1) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
	}

}

?>
