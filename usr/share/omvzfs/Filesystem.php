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
		$cmd = "zfs list -p -H -t filesystem \"" . $name . "\" 2>&1";
		try {
			OMVModuleZFSUtil::exec($cmd, $out, $res);
		}
		catch (\OMV\ExecException $e) {

		}
	}

	public function exists(){
		try {
			$cmd = "zfs list -p -H -t filesystem \"" . $this->name . "\" 2>&1";
			OMVModuleZFSUtil::exec($cmd, $out, $res);
			return TRUE;
		}
		catch (\OMV\ExecException $e) {
			return FALSE;
		}
	}

	public static function getAllFilesystems(){
		$cmd = "zfs list -p -H -o name,mountpoint -t filesystem | awk '$2 != \"/\" && $2 != \"/boot\" { print $0 }'";
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
		$cmd="zfs list -p -H -r -t filesystem \"$name\" 2>&1";
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

	/**
	 * Get encryption status of the Dataset
	 *
	 * @return array encryption status information
	 * @access public
	 */
	public function getEncryptionStatus() {
		$properties = [
			'encryption',
			'encryptionroot',
			'keystatus',
			'keyformat'
		];
		$cmd = "zfs get -H -p " . implode(',', $properties) . " \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd, $out, $res);

		$status = [];
		foreach ($out as $line) {
			if (empty($line)) continue;
			$parts = preg_split('/\t+/', $line);
			if (count($parts) >= 3) {
				$status[$parts[1]] = [
					'value' => $parts[2],
					'source' => isset($parts[3]) ? $parts[3] : '-'
				];
			}
		}
		return $status;
	}

	/**
	 * Load encryption key for the Dataset
	 *
	 * @param string $key The encryption key (passphrase or hex)
	 * @param string $keyformat Format of the key (passphrase or hex)
	 * @return void
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public function loadEncryptionKey($key, $keyformat = 'passphrase') {
		$cmd = "zfs load-key \"" . $this->name . "\"";
		$descriptors = [
			0 => ['pipe', 'r'],
			1 => ['pipe', 'w'],
			2 => ['pipe', 'w']
		];
		$process = proc_open($cmd, $descriptors, $pipes);
		if (!is_resource($process)) {
			throw new OMVModuleZFSException("Failed to start zfs load-key process");
		}
		fwrite($pipes[0], $key . "\n");
		fclose($pipes[0]);
		$stdout = stream_get_contents($pipes[1]);
		$stderr = stream_get_contents($pipes[2]);
		fclose($pipes[1]);
		fclose($pipes[2]);
		$result = proc_close($process);
		if ($result !== 0) {
			throw new OMVModuleZFSException("Failed to load encryption key: " . trim($stderr ?: $stdout));
		}
	}

	/**
	 * Unload encryption key for the Dataset
	 *
	 * @return void
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public function unloadEncryptionKey() {
		$cmd = "zfs unload-key \"" . $this->name . "\" 2>&1";
		OMVModuleZFSUtil::exec($cmd, $out, $res);
	}

	/**
	 * Change encryption key for the Dataset
	 *
	 * @param string $oldkey The current encryption key
	 * @param string $newkey The new encryption key
	 * @param string $keyformat Format of the keys (passphrase or hex)
	 * @return void
	 * @throws OMVModuleZFSException
	 * @access public
	 */
	public function changeEncryptionKey($oldkey, $newkey, $keyformat = 'passphrase') {
		// First load with old key (in case it's not loaded)
		try {
			$this->loadEncryptionKey($oldkey, $keyformat);
		} catch (\Exception $e) {
			// Key might already be loaded, continue
		}

		// Change the key
		$cmd = "zfs change-key \"" . $this->name . "\"";
		$descriptors = [
			0 => ['pipe', 'r'],
			1 => ['pipe', 'w'],
			2 => ['pipe', 'w']
		];
		$process = proc_open($cmd, $descriptors, $pipes);
		if (!is_resource($process)) {
			throw new OMVModuleZFSException("Failed to start zfs change-key process");
		}
		fwrite($pipes[0], $newkey . "\n");
		fclose($pipes[0]);
		$stdout = stream_get_contents($pipes[1]);
		$stderr = stream_get_contents($pipes[2]);
		fclose($pipes[1]);
		fclose($pipes[2]);
		$result = proc_close($process);
		if ($result !== 0) {
			throw new OMVModuleZFSException("Failed to change encryption key: " . trim($stderr ?: $stdout));
		}
	}

}

?>
