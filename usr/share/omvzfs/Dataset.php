<?php
require_once("Utils.php");
require_once("Snapshot.php");
trait Cloneable {
	/**
	* Check if the Dataset is a clone or not.
	*
	* @return bool
	* @access public
	*/
	public function isClone() {
		$origin = $this->properties["origin"];
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
			$origin = $this->properties["origin"];
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
			OMVModuleZFSUtil::exec($cmd,$out,$res);
		}
	}
}

abstract class OMVModuleZFSDataset {

    // Attributes
    /**
     * Name of Dataset
     *
     * @var    string $name
     * @access private
     */
    protected $name;

    /**
     * Array with properties assigned to the Dataset
     *
     * @var    array $properties
     * @access private
     */
    protected $properties;


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
			OMVModuleZFSUtil::exec($cmd,$out,$res);
			$this->updateProperty($newpropertyk);
		}
	}

	/**
	 * Get all Dataset properties from commandline and update object properties attribute
	 *
	 * @return void
	 * @access private
	 */
	public function updateAllProperties() {
		$cmd = "zfs get -H all \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
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
	public function updateProperty($property) {
		$cmd = "zfs get -H " . $property . " \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$tmpary = preg_split('/\t+/', $out[0]);
		$this->properties["$tmpary[1]"] = array("value" => $tmpary[2], "source" => $tmpary[3]);
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
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$this->name = $newname;
	}

	/**
	 * Destroy the Dataset.
	 *
	 * @return void
	 * @access public
	 */
	public function destroy() {
		$cmd = "zfs destroy \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
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
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$this->updateProperty($property);
	}

	/**
	 * Sets a custom property for storing mntent uuid. 
	 *
	 * @param  string internal database uuid of the mntent entry.
	 * @return void
	 * @access public
	 */
	public function setMntentProperty($mntent_uuid) {
		$cmd = "zfs set " . "omvzfsplugin:uuid" . "=\"" . $mntent_uuid . "\" \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
	}


}