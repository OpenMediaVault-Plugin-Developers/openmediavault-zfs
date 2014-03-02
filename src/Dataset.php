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
	 * @param array $features An array of features (strings) in the form <key>=<value> to set when creating the Dataset
	 * @throws OMVModuleZFSException
	 *
	 */
	public function __construct($name, array $features = null) {
		$cmd = "zfs create ";
		if (isset($features)) {
			foreach ($features as $feature) {
				$cmd .= "-o " . $feature . " ";
			}
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
	 * Sets a number of Dataset properties. If a property is already set it will be updated with the new value.
	 *
	 * @param  array $features An array of strings in format <key>=<value>
	 * @return void 
	 * @access public
	 */
	public function setFeatures($features) {
		foreach ($features as $newfeature) {
			$cmd = "zfs set " . $newfeature . " " . $this->name;
			exec($cmd,$out,$res);
			if ($res == 1) {
				throw new OMVModuleZFSException(implode("\n", $out));
			}
			$tmp = explode("=", $newfeature);
			$newfeaturek = $tmp[0];
			$found = false;
			for ($i=0; $i<count($this->features); $i++) {
				$tmp = explode("=", $this->features[$i]);
				$oldfeaturek = $tmp[0];
				if (strcmp($newfeaturek, $oldfeaturek) == 0) {
					$this->features[$i] = $newfeature;
					$found = true;
					continue;
				}
			}
			if (!$found) {
				array_push($this->features, $newfeature);
			}
		}
	}

	/**
	 * Destroy the Dataset.
	 *
	 */
	public function destroy() {
		$cmd = "zfs destroy " . $this->name;
		exec($cmd,$out,$res);
		if ($res == 1) {
			throw new OMVModuleZFSException(implode("\n", $out));
		}
	}

}

?>
