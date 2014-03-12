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
	 * Array with properties assigned to the Zvol
	 * 
	 * @var    array $properties
	 * @access private
	 */
	private $properties;

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
		$this->name = $name;
		$qname = preg_quote($name, '/');
		$cmd = "zfs list -H -t volume";
		$this->exec($cmd, $out, $res);
		foreach ($out as $line) {
			if (preg_match('/^' . $qname . '\t.*$/', $line)) {
				$this->updateAllProperties();
				$this->size = $this->properties["volsize"]["value"];
				continue;
			}
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
		return $this->size;
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
			$cmd = "zfs set " . $newpropertyk . "=" . $newpropertyv . " " . $this->name;
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
		$cmd = "zfs get -H all " . $this->name;
		$this->exec($cmd,$out,$res);
		unset($this->properties);
		foreach ($out as $line) {
			$tmpary = preg_split('/\t+/', $line);
			$this->properties["$tmpary[1]"] = array("value" => $tmpary[2], "source" => $tmpary[3]);
		}
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
		if (isset($sparse) && $sparse == true) {
			$cmd .= "-s ";
		}	
		$cmd .= "-V " . $size . " " . $this->name . " 2>&1";
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
		$cmd = "zfs destroy " . $this->name;
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
