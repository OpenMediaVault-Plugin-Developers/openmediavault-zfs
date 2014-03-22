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
		$this->name = $name;
		$qname = preg_quote($name, '/');
		$cmd = "zfs list -H -t snapshot";
		$this->exec($cmd, $out, $res);
		foreach ($out as $line) {
			if (preg_match('/^' . $qname . '\t.*$/', $line)) {
				$this->updateAllProperties();
				continue;
			}
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
	 * XXX
	 *
	 * @return list<Feature> XXX
	 * @access public
	 */
	public function getFeatures() {
		trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @param   $list<Feature> XXX
	 * @return void XXX
	 * @access public
	 */
	public function setFeatures($list) {
		trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * Get all Snapshot properties from commandline and update object properties attribute
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
