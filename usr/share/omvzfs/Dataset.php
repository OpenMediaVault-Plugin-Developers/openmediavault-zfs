<?php
require_once("Exception.php");
require_once("openmediavault/util.inc");
require_once("Snapshot.php");

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

	/**
	 * Array with Snapshots associated to the Dataset
	 *
	 * @var 	array $snapshots
	 * @access private
	 */
	private $snapshots;

	// Associations
	// Operations

	/**
	 * Constructor. If the Dataset already exists in the system the object will be updated with all
	 * associated properties from commandline.
	 *
	 * @param string $name Name of the new Dataset
	 * @return void
	 * @access public
	 */
	public function __construct($name) {
		$ds_exists = true;
		$this->name = $name;
		$cmd = "zfs list -H -t filesystem \"" . $name . "\" 2>&1";
		try {
			$this->exec($cmd, $out, $res);
			$this->updateAllProperties();
			$this->mountPoint = $this->properties["mountpoint"]["value"];
		}
		catch (OMVModuleZFSException $e) {
			$ds_exists = false;
		}
		if ($ds_exists) {
			$cmd = "zfs list -r -d 1 -o name -H -t snapshot \"" . $name . "\" 2>&1";
			$this->exec($cmd, $out2, $res2);
			foreach ($out2 as $line2) {
					$this->snapshots[$line2] = new OMVModuleZFSSnapshot($line2);
			}
		} else {
			$this->create();
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
	 * Get all Snapshots associated with the Dataset
	 *
	 * @return array $snapshots
	 * @access public
	 */
	public function getSnapshots() {
		if (isset($this->snapshots)) {
			return $this->snapshots;
		} else {
			return array();
		}
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
		return $this->properties["$property"];
	}

	/**
	 * Get an associative array of all properties associated with the Snapshot
	 * 
	 * @return array $properties Each entry is an associative array with two elements
	 * <value> and <source>
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
			$cmd = "zfs set " . $newpropertyk . "=\"" . $newpropertyv . "\" \"" . $this->name . "\" 2>&1";
			$this->exec($cmd,$out,$res);
			$this->updateProperty($newpropertyk);
		}
	}

	/**
	 * Get all Dataset properties from commandline and update object properties attribute
	 *
	 * @return void
	 * @access private
	 */ 
	private function updateAllProperties() {
		$cmd = "zfs get -H all \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		unset($this->properties);
		foreach ($out as $line) {
			$tmpary = preg_split('/\t+/', $line);
			$this->properties["$tmpary[1]"] = array("value" => $tmpary[2], "source" => $tmpary[3]);
		}
	}

	/**
	 * Get single Datset property from commandline and update object property attribute
	 *
	 * @param string $property Name of the property to update
	 * @return void
	 * @access private
	 */
	private function updateProperty($property) {
		$cmd = "zfs get -H " . $property . " \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$tmpary = preg_split('/\t+/', $out[0]);
		$this->properties["$tmpary[1]"] = array("value" => $tmpary[2], "source" => $tmpary[3]);
	}

	/**
	 * Craete a Dataset on commandline.
	 *
	 * @return void
	 * @access private
	 */
	private function create() {
		$cmd = "zfs create -p \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->updateAllProperties();
		$this->mountPoint = $this->properties["mountpoint"]["value"];
	}

	/**
	 * Destroy the Dataset.
	 *
	 * @return void
	 * @access public
	 */
	public function destroy() {
		$cmd = "zfs destroy \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Renames a Dataset
	 *
	 * @param string $newname New name of the Dataset
	 * @return void
	 * @access public
	 */
	public function rename($newname) {
		$cmd = "zfs rename -p \"" . $this->name . "\" \"" . $newname . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->name = $newname;
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
		$this->updateProperty($property);
	}

	/**
	 * Upgrades the Dataset to latest filesystem version
	 *
	 * @return void
	 * @access public
	 */
	public function upgrade() {
		$cmd = "zfs upgrade \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Mount the Dataset
	 *
	 * @return void
	 * @access public
	 */
	public function mount() {
		$cmd = "zfs mount \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->updateProperty("mounted");
	}

	/**
	 * Unmount the Dataset
	 *
	 * @return void
	 * @access public
	 */
	public function unmount() {
		$cmd = "zfs unmount \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->updateProperty("mounted");
	}

	/**
	 * Creates a Snapshot and adds it to the existing list of snapshots associated
	 * with the Dataset.
	 *
	 * @param string $snap_name Name of the Snapshot to create.
	 * @param array $properties Optional array of properties to set on Snapshot
	 * @return void
	 * @access public
	 */
	public function addSnapshot($snap_name, array $properties = null) {
		$snap = new OMVModuleZFSSnapshot($snap_name);
		$snap->create($properties);
		$this->snapshots[$snap_name] = $snap;
	}

	/**
	 * Destroys a Snapshot on commandline and removes it from the Dataset.
	 *
	 * @param string $snap_name Name of the Snapshot to delete.
	 * @return void
	 * @access public
	 */
	public function deleteSnapshot($snap_name) {
		$this->snapshots[$snap_name]->destroy();
		unset($this->snapshots[$snap_name]);
	}

	/**
	* Check if the Dataset is a clone or not.
	*
	* @return bool
	* @access public
	*/
	public function isClone() {
		$origin = $this->getProperty("origin");
		if (strlen($origin["value"]) > 0) {
			return true;
		} else {
			return false;
		}
	}

	/**
	* Get the origin of the Dataset if it's a clone.
	*
	* @return string The name of the origin if it exists. Otherwise an empty string.
	* @access public
	*/
	public function getOrigin() {
		if ($this->isClone()) {
			$origin = $this->getProperty("origin");
			return $origin['value'];
		} else {
			return "";
		}
	}

	/**
	* Promotes the Dataset if it's a clone.
	*
	* @return void
	* @access public
	*/
	public function promote() {
		if ($this->isClone()) {
			$cmd = "zfs promote \"" . $this->name . "\" 2>&1";
			$this->exec($cmd,$out,$res);
		}
	}

	/**
	 * Helper function to execute a command and throw an exception on error
	 * (requires stderr redirected to stdout for proper exception message).
	 *
	 * @param string $cmd Command to execute
	 * @param array &$out If provided will contain output in an array
	 * @param int &$res If provided will contain Exit status of the command
	 * @return string Last line of output when executing the command
	 * @throws OMVModuleZFSException
	 * @access private
	 */
	private function exec($cmd, &$out = null, &$res = null) {
		$tmp = OMVUtil::exec($cmd, $out, $res);
		if ($res) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		return $tmp;
	}

}

?>
