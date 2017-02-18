<?php
require_once("Dataset.php");
require_once("Exception.php");


/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSSnapshot extends OMVModuleZFSDataset {
    // Attributes

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


	public function __construct($name){
		$this->name = $name;
	}

	/**
	 * Create a Snapshot on commandline.
	 * 
	 * @return void
	 * @access private
	 */
	public static function create($name) {
		$cmd = "zfs snapshot \"" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return new OMVModuleZFSSnapshot($name);
	}
	
	public static function getAllSnapshots() {
		$cmd = "zfs snapshot \"" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return new OMVModuleZFSSnapshot($name);
	}

	/**
	 * Rollback a Snapshot on commandline.
	 *
	 * @return void
	 * @access public
	 */
	public function rollback() {
		$cmd = "zfs rollback \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
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
		OMVModuleZFSUtil::exec($cmd,$out,$res);
	}

}

?>
