<?php
require_once("Exception.php");

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
     * Size of Dataset
     *
     * @var    int $size
     * @access private
     */
    private $size;

    /**
     * Mountpoint of the Dataset
     *
     * @var    string $mountPoint
     * @access private
     */
    private $mountPoint;

    /**
     * List of features assigned to the Dataset
     *
     * @var    array $features
     * @access private
     */
    private $features;

	// Associations
	// Operations

	/**
	 * Constructor
	 * 
	 * @param string $name Name of the new Dataset
	 * @param array $features An array of features to set when creating the Dataset
	 *
	 */
	public function __construct($name, array $features = null) {
		$cmd = "zfs create ";
		if (isset($features)) {
			$cmd .= "-o " . implode(",", $features) . " ";
		}
		$cmd .= $name . " 2>&1";
		exec($cmd,$out,$res);
		if ($res == 1) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
		unset($res);
		$this->name = $name;
		if (isset($features)) {
			$this->features = $features;
			foreach ($features as $feature) {
				if (preg_match('/^mountpoint\=(.*)$/', $feature, $res)) {
					$this->mountPoint = $res[1];
					continue;
				}
			}
		} else {
			$this->features = array();
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
	 * Get the size of the Dataset
	 *
	 * @return int $size
	 * @access public
	 */
	public function getSize() {
		return $this->size;
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
	 * Get an array of features associated with the Dataset
	 *
	 * @return array $features
	 * @access public
	 */
	public function getFeatures() {
		return $this->features;
	}

	/**
	 * XXX
	 *
	 * @param  array XXX
	 * @return void XXX
	 * @access public
	 */
	public function setFeatures($list) {
		trigger_error('Not Implemented!', E_USER_WARNING);
	}

}

?>
