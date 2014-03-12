<?php

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
	 * @var int $size
	 * @access private
	 */
	private $size;

	/**
	 * Mountpoint of the Zvol
	 *
	 * @var    string $mountPoint
	 * @access private
	 */
	private $mountPoint;

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
	 * Return name of the Zvol
	 * 
	 * @return string $name
	 * @access public
	 */
	public function getName() {
		return $this->name;
	}

	/**
	 * Get the mountpoint of the Zvol
	 * 
	 * @return string $mountPoint
	 * @access public
	 */
	public function getMountPoint() {
		return $this->mountPoint;
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
	 * XXX
	 *
	 * @return int XXX
	 * @access public
	 */
	public function getSize() {
		trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @param   $list<Feature> XXX
	 * @return void XXX
	 * @access public
	 */
	public function setProperties($properties) {
		trigger_error('Not Implemented!', E_USER_WARNING);
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
