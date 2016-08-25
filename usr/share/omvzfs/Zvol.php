<?php
use OMV\System\Process;
require_once("Snapshot.php");
require_once("Utils.php");

/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSZvol {
	// Attributes
	
	/**
	 * Name of Zvol
	 *
	 * @var string $name
	 * @access private
	 */
	private $name;

	/**
	 * Size of Zvol
	 *
	 * @var string $size
	 * @access private
	 */
	private $size;

	/**
	 * Used of Zvol
	 *
	 * @var string $used
	 * @access private
	 */
	private $used;

	/**
	 * Array with properties assigned to the Zvol
	 * 
	 * @var    array $properties
	 * @access private
	 */
	private $properties;

	/**
	 * Array with Snapshots associated to the Zvol
	 * 
	 * @var     array $snapshots
	 * @access private
	 */
	private $snapshots;

	// Associations
	// Operations

	/**
	 * Constructor. If the Zvol already exists in the system the object will be updated with all
	 * associated properties from commandline.
	 * 
	 * @param string $name Name of the new Zvol
	 * @return void
	 * @access public
	 */
	public function __construct($name) {
		$zvol_exists = true;
		$this->name = $name;
		$cmd = "zfs list -H -t volume \"" . $name . "\" 2>&1";
		try {
			$this->exec($cmd, $out, $res);
			$this->updateAllProperties();
		}
		catch (\OMV\ExecException $e) {
			$zvol_exists = false;
		}
		if ($zvol_exists) {
			$cmd = "zfs list -r -d 1 -o name -H -t snapshot \"" . $name . "\" 2>&1";
			$this->exec($cmd, $out2, $res2);
			foreach ($out2 as $line2) {
				$this->snapshots["$line2"] = new OMVModuleZFSSnapshot($line2);
			}
			$cmd = "zfs get -o value -Hp volsize,logicalused \"$name\" 2>&1";
			$this->exec($cmd, $out3, $res3);
			$this->size = $out3[0];
			$this->used = $out3[1];
		}

	}

	/**
	 * Return name of the Zvol
	 * 
	 * @return string $name
	 * @access public
	 */
	public function getName() {
		return $this->name;
	}

	/**
	 * Get a single property value associated with the Zvol
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
	 * Get an associative array of all properties associated with the Zvol
	 * 
	 * @return array $properties Each entry is an associative array with two elements
	 * <value> and <source>
	 * @access public
	 */
	public function getProperties() {
		return $this->properties;
	}

	/**
	 * Get the total size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getSize() {
		return OMVModuleZFSUtil::bytesToSize($this->size);
	}

	/**
	 * Get the available size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getAvailable() {
		return OMVModuleZFSUtil::bytesToSize($this->size - $this->used);
	}

	/**
	 * Get the used size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getUsed() {
		return OMVModuleZFSUtil::bytesToSize($this->used);
	}

	/**
	 * Get all Snapshots associated with the Zvol
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
	 * Sets a number of Zvol properties. If a property is already set it will be updated with the new value.
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
	 * Get all Zvol properties from commandline and update object properties attribute
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
	 * Create a Zvol on commandline. Optionally provide a number of properties to set.
	 * 
	 * @param string $size Size of the Zvol that should be created
	 * @param array $properties Properties to set when creatiing the dataset.
	 * @param boolean $sparse Defines if a sparse volume should be created.
	 * @return void
	 * @access public
	 */
	public function create($size, array $properties = null, $sparse = null) {
		$cmd = "zfs create -p ";
		if ((isset($sparse)) && ($sparse == true)) {
			$cmd .= "-s ";
		}	
		$cmd .= "-V " . $size . " \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
		$this->updateAllProperties();
		$this->setProperties($properties);
		$this->size = $this->properties["volsize"]["value"];
	}

	/**
	 * Destroy the Zvol.
	 * 
	 * @return void
	 * @access public
	 */
	public function destroy() {
		$cmd = "zfs destroy \"" . $this->name . "\" 2>&1";
		$this->exec($cmd,$out,$res);
	}

	/**
	 * Renames a Zvol
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
	 * Creates a Snapshot and adds it to the existing list of snapshots associated
	 * with the Zvol.
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
	 * Destroys a Snapshot on commandline and removes it from the Zvol.
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
	* Check if the Volume is a clone or not.
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
	* Get the origin of the Volume if it's a clone.
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
	* Promotes the Volume if it's a clone.
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
	 * @throws \OMV\ExecException
	 * @access private
	 */
	private function exec($cmd, &$out = null, &$res = null) {
		$process = new Process($cmd);
		$tmp = $process->execute($out,$res);
		return $tmp;
	}
}

?>
