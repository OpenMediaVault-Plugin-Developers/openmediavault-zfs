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
class OMVModuleZFSSnapshot {
    // Attributes
    /**
     * Name of the Snapshot
     *
     * @var    string $name
     * @access private
     */
    private $name;

    /**
     * Properties associated with the Snaphost
     *
     * @var    array $properties
     * @access private
     */
    private $properties;

    // Associations
    // Operations

	/**
	 * Constructor. If the Snapshot already exists in the system the object will be updated with all
	 * associated properties from commandline.
	 * 
	 * @param string $name Name of the new Snapshot
	 * @return void
	 * @access public
	 */
	public function __construct($name) {
		$snap_exists = true;
		$this->name = $name;
		$cmd = "zfs list -H -t snapshot \"" .$name . "\" 2>&1";
		try {
			$this->exec($cmd, $out, $res);
			$this->updateAllProperties();
		}
		catch (OMVModuleZFSException $e) {
			$snap_exists = false;
		}
		if (!$snap_exists) {
			$this->create();
		}
	}

	/**
	 * Return name of the Snapshot
	 *
	 * @return string Nameof the Snapshot
	 * @access public
	 */
	public function getName() {
		return $this->name;
	}

	/**
	 * Get a single property value associated with the Snapshot
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
	 * Sets a number of Snapshot properties. If a property is already set it will be updated with the new value.
	 * 
	 * @param  array $properties An associative array with properties to set
	 * @return void
	 * @access public
	 */
	public function setProperties($properties) {
		foreach ($properties as $newpropertyk => $newpropertyv) {
			$cmd = "zfs set " . $newpropertyk . "=" . $newpropertyv . " \"" . $this->name . "\" 2>&1";
			$this->exec($cmd,$out,$res);
			$this->updateProperty($newpropertyk);
		}
	}

	/**
	 * Get single Snapshot property from commandline and update object property attribute
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
	 * Get all Snapshot properties from commandline and update object properties attribute
	 * 
	 * @return void
	 * @access private
	 */
	private function updateAllProperties() {
		$cmd = "zfs get -H all \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		unset($this->properties);
		foreach ($out as $line) {
			$tmpary = preg_split('/\t/', $line);
			if (strlen($tmpary[2] == 0)) {
				$this->properties["$tmpary[1]"] = array("value" => "-", "source" => $tmpary[3]);
			} else {
				$this->properties["$tmpary[1]"] = array("value" => $tmpary[2], "source" => $tmpary[3]);
			}
		}
	}

	/**
	 * Create a Snapshot on commandline.
	 * 
	 * @return void
	 * @access private
	 */
	private function create() {
		$cmd = "zfs snapshot \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->updateAllProperties();
	}

	/**
	 * Destroy a Snapshot on commandline.
	 *
	 * @return void
	 * @access public
	 */
	public function destroy() {
		$cmd = "zfs destroy \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Rollback a Snapshot on commandline.
	 *
	 * @return void
	 * @access public
	 */
	public function rollback() {
		$cmd = "zfs rollback \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Clones a Snapshot on commandline.
	 *
	 * @param string $newname
	 * @return void
	 * @access public
	 */
	public function clonesnap($newname) {
		$cmd = "zfs clone -p \"" . $this->name . "\" \"" . $newname . "\" 2>&1";
		$this->exec($cmd,$out,$res);
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
