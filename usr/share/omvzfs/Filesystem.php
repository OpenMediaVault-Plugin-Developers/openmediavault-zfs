<?php
require_once("Dataset.php");


/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSFilesystem extends OMVModuleZFSDataset {

	use Cloneable;

	// Associations
	// Operations

	/**
	 * Constructor. If the Dataset already exists in the system the object will be updated with all
	 * associated properties from commandline.
	 *
	 * @param string $name Name of the new Dataset
	 * @return void
	 * @access public
	 */
	public function __construct($name) {
		$this->name = $name;
		$cmd = "zfs list -H -t filesystem \"" . $name . "\" 2>&1";
		try {
			OMVModuleZFSUtil::exec($cmd, $out, $res);
		}
		catch (\OMV\ExecException $e) {

		}
	}

	public static function getAllFilesystems(){
		$cmd = "zfs list -H -o name,mountpoint -t filesystem";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
		$filesystems=[];
		foreach($out as $line) {
			$tmpary = preg_split('/\t+/', $line);
			$filesystems[]=new OMVModuleZFSFilesystem($tmpary[0]);
		}
		return $filesystems;
	}


	/**
	 * Craete a Dataset on commandline.
	 *
	 * @return OMVModuleZFSFilesystem
	 * @access public
	 */
	public static function create($name) {
		$cmd = "zfs create -p \"" . $name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		return new OMVModuleZFSFilesystem($name);
	}

    /**
     *
     * @return int
     * @access public
     */
    public function getUsed() {
        return OMVModuleZFSUtil::SizeTobytes($this->properties["used"]["value"]);
    }

    /**
     *
     * @return int
     * @access public
     */
    public function getAvailable() {
        return OMVModuleZFSUtil::SizeTobytes($this->properties["available"]["value"]);
    }

    /**
     * XXX
     *
     * @return int
     * @access public
     */
    public function getSize() {
        $used = $this->getUsed();
        $avail = $this->getAvailable();
        return $avail + $used;
    }

	/**
	 * Get the mountpoint of the Dataset
	 *
	 * @return string $mountPoint
	 * @access public
	 */
	public function getMountPoint() {
		return $this->properties["mountpoint"]["value"];
	}


	public function getChildren(){
		$name = $this->name;
		$cmd="zfs list -H -r -t filesystem $name 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$children=[];
		foreach ($out as $line) {
			$tmpary = preg_split('/\t+/', $line);
			$children[]=new OMVModuleZFSFilesystem($tmpary[0]);
		}
		return $children;
	}
	/**
	 * Upgrades the Dataset to latest filesystem version
	 *
	 * @return void
	 * @access public
	 */
	public function upgrade() {
		$cmd = "zfs upgrade \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
	}

	/**
	 * Mount the Dataset
	 *
	 * @return void
	 * @access public
	 */
	public function mount() {
		$cmd = "zfs mount \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$this->updateProperty("mounted");
	}

	/**
	 * Unmount the Dataset
	 *
	 * @return void
	 * @access public
	 */
	public function unmount() {
		$cmd = "zfs unmount \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd,$out,$res);
		$this->updateProperty("mounted");
	}

}

?>
