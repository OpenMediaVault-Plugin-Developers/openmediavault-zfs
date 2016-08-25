<?php
require_once("Exception.php");
require_once("VdevType.php");
use OMV\System\Process;

/**
 * Contains a Vdev
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 */
class OMVModuleZFSVdev {
    // Attributes
    /**
     * Array holding disks
     *
     * @var    array $disks
     * @access private
     */
    private $disks;

    /**
     * Name of pool
     *
     * @var    string $pool pool name
     * @access private
     */
    private $pool;

    /**
     * This vdev type
     *
     * @var    OMVModuleZFSVdevType $type Vdev type
     * @access private
     */
    private $type;

    // Associations
    // Operations
	/**
	 * Constructor
	 *
	 * @param $pool pool this mirror belongs to
     * @throws OMVModuleZFSException
	 */

	public function __construct($pool, $type, array $disks) {
		switch ($type) {
			case OMVModuleZFSVdevType::OMVMODULEZFSPLAIN:
				break;
			case OMVModuleZFSVdevType::OMVMODULEZFSMIRROR:
				if (count($disks) < 2)
					throw new OMVModuleZFSException("A mirror must contain at least 2 disks");
				break;
			case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1:
				if (count($disks) < 3)
					throw new OMVModuleZFSException("A Raidz1 must contain at least 3 disks");
				break;
			case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2:
				if (count($disks) < 4)
					throw new OMVModuleZFSException("A Raidz2 must contain at least 4 disks");
				break;
			case OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3:
				if (count($disks) < 5)
					throw new OMVModuleZFSException("A Raidz3 must contain at least 5 disks");
				break;
			default:
				throw new OMVModuleZFSException("$type: Unknown zpool type");
		}
		$this->pool = $pool;
		$this->disks = $disks;
		$this->type = $type;
	}

	/**
	 * Helper function to execute an external program.
	 * @param command The command that will be executed.
	 * @param output If the output argument is present, then the specified
	 *   array will be filled with every line of output from the command.
	 *   Trailing whitespace, such as \n, is not included in this array.
	 * @return The exit code of the command.
	 * @throws E_EXEC_FAILED
	 */
	private function exec($command, &$output = NULL) {
		$process = new Process($cmd);
		$process->execute($output,$result);
		return $result;
	}

	private function safeExec($disk, $add = true, $change = false) {
		$result = 1;

		if ($add) {
			if ($change || $this->type == OMVModuleZFSVdevType::OMVMODULEZFSMIRROR) {
				$disk1 = $this->disks[0];
				$result = exec("zpool attach {$this->pool} $disk1 $disk", $err);
			} else {
				$result = exec("zpool add {$this->pool} $disk", $err);
			}
		} else {
			if ($this->type == OMVModuleZFSVdevType::OMVMODULEZFSMIRROR) {
				$disk1 = $this->disks[0];
				if (($res = exec("zpool offline {$this->pool} $disk", $err)) > 0)
					$result = $res;
				else
					$result = exec("zpool detach {$this->pool} $disk", $err);
			} else {
				$result = 1;
				$err = "Cannot remove $disk from {$this->pool}";
			}
		}

		return ($result) ? $err : null;
	}

    /**
     * Add a disk to this Vdev
     *
     * @param  $disk the disk
     * @throws OMVModuleZFSException
     * @access public
     */
    public function addDisk($disk, $changeType = false) {
		if ($this->type != OMVModuleZFSVdevType::OMVMODULEZFSPLAIN ||
				$this->type != OMVModuleZFSVdevType::OMVMODULEZFSMIRROR)
			throw new OMVModuleZFSException("A Raidz Vdev cannot be changed");

		if (in_array($disk, $this->disks))
			throw new OMVModuleZFSException("$disk: Already part of Vdev");

		if ($this->type == OMVModuleZFSVdevType::OMVMODULEZFSPLAIN &&
				count($this->disks) < 2 && $changeType) {
			$this->type = OMVModuleZFSVdevType::OMVMODULEZFSMIRROR;
		}

		if (($err = safeExec($disk, true, $changeType)) != null)
			throw new OMVModuleZFSException($err);
		else
			array_push($this->disks, $disk);
    }

    /**
     * Remove a disk from Vdev
     *
     * @param  $disk disk to remove
     * @throws OMVModuleZFSException
     * @access public
     */
    public function removeDisk($disk, $changeType = false) {
		$new_disks = array();

		if ($this->type != OMVModuleZFSVdevType::OMVMODULEZFSMIRROR)
			throw new OMVModuleZFSException("Only inactive hot spares," .
				"cache, top-level, or log devices can be removed");

		if (count($this->disks) < 3 && ! $changeType)
			throw new OMVModuleZFSException("A mirror must contain at least 2 disks");

		if (! in_array($disk, $this->disks))
			throw new OMVModuleZFSException("$disk: Not part of Vdev");

		if (($err = safeExec($disk, false, $changeType)) != null)
			throw new OMVModuleZFSException($err);
		else {
			foreach ($this->disks as $_disk) {
				if (strcmp($_disk, $disk) != 0)
					array_push($new_disks, $_disk);
			}
		}

		$this->disks = $new_disks;
	}

    /**
     * Get disk array
     *
     * @return array with disks
     * @access public
     */
    public function getDisks() {
        return $this->disks;
    }

    /**
     * Get pool
     *
     * @return string pool
     * @access public
     */
    public function getPool() {
        return $this->pool;
    }

    /**
     * Get type
     *
     * @return OMVModuleZFSVdevType
     * @access public
     */
    public function getType() {
        return $this->type;
    }

}

?>
