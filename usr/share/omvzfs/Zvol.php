<?php
use OMV\System\Process;
require_once("Dataset.php");
require_once("Snapshot.php");
require_once("Utils.php");

/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSZvol extends OMVModuleZFSDataset {

	use Cloneable;


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
		$cmd = "zfs list -p -H -t volume \"" . $name . "\" 2>&1";
		try {
			OMVModuleZFSUtil::exec($cmd, $out, $res);
		}
		catch (\OMV\ExecException $e) {
			$zvol_exists = false;
		}
	}

	/**
	 * Get the total size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getSize() {
		return OMVModuleZFSUtil::SizeTobytes($this->properties["volsize"]["value"]);
	}

	/**
	 * Get the available size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getAvailable() {
		$size=$this->getSize();
		$used=$this->getUsed();
		return $size - $used;
	}

	/**
	 * Get the used size of the Zvol
	 *
	 * @return string $size
	 * @access public
	 */
	public function getUsed() {
		return OMVModuleZFSUtil::SizeTobytes($this->properties["logicalused"]["value"]);
	}

	/**
	 * Returns TRUE or FALSE depending on if the volume is thin provisioned or not.
	 *
	 */
	public function isThinVol() {
		$property = $this->getProperty("refreservation");
		if (strcmp($property['value'], "none") === 0)
			return TRUE;
		return FALSE;
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

	public static function create($name, $size, array $properties = null, $sparse = null) {
		$cmd = "zfs create -p ";
		if ((isset($sparse)) && ($sparse == true)) {
			$cmd .= "-s ";
		}
		$cmd .= "-V " . $size . " \"" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return new self($name);
	}

}

?>
