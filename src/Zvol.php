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
	 * XXX
	 *
	 * @return string XXX
	 * @access public
	 */
	public function getName() {
		trigger_error('Not Implemented!', E_USER_WARNING);
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
	 * @return string XXX
	 * @access public
	 */
	public function getMountPoint() {
		trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @return list<Feature> XXX
	 * @access public
	 */
	public function getProperties() {
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
