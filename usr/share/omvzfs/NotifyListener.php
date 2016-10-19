<?php
use OMV\Engine\Module\ServiceAbstract;
use OMV\Engine\Module\Manager;
use OMV\Engine\Notify\IListener;
use OMV\Engine\Notify\Dispatcher;

require_once("Exception.php");

/**
 * Class containing information about the pool
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 */
class OMVModuleZFS extends ServiceAbstract
		implements IListener {
	use OMV\Debugable;

	private $pools;

	public function __construct() {
		$this->pools = [];
		$this->updatePools();
	}

	public function getName() {
		return "zfs";
	}

    public function bindListeners(Dispatcher $dispatcher) {
		$moduleMgr = Manager::getInstance();

		// Update service if configuration has been modified
		$dispatcher->addListener(
		  OMV_NOTIFY_MODIFY,
		  "org.openmediavault.services.nfs",
		  array($this, "onUpdateNFSService"));
		$dispatcher->addListener(
		  OMV_NOTIFY_CREATE,
		  "org.openmediavault.services.nfs.shares.share",
		  array($this, "onCreateNFSShare"));
		$dispatcher->addListener(
		  OMV_NOTIFY_DELETE,
		  "org.openmediavault.services.nfs.shares.share",
		  array($this, "onDeleteNFSShare"));
		$dispatcher->addListener(
		  OMV_NOTIFY_MODIFY,
		  "org.openmediavault.services.nfs.shares.share",
		  array($this, "onUpdateNFSShare"));
		  
		// Make sure we notify NFS and fstab when we get updated.
		$dispatcher->addListener(
          OMV_NOTIFY_MODIFY,
          "org.openmediavault.storage.zfs.filesystem",
          array($moduleMgr->getModule("nfs"), "setDirty"));
		/*
        $dispatcher->addListener(
          OMV_NOTIFY_MODIFY,
          "org.openmediavault.storage.zfs.filesystem",
          array($moduleMgr->getModule("fstab"), "setDirty"));
		*/ 
		$this->debug(sprintf("bindListeners %s", var_export($dispatcher, true)));
    }

	/**
	 * XXX
	 * org.openmediavault.services.nfs
	 *
	 * @param string event
	 * @access public
	 */
	public function onUpdateNFSService($args) {
        $this->debug(sprintf("onUpdateNFSService args=%s", var_export($args, true)));
	}

	/**
	 * XXX
	 * org.openmediavault.services.nfs.shares.share
	 *
	 * @param string event
	 * @access public
	 */
	public function onCreateNFSShare($args) {
        $this->debug(sprintf("onCreateNFSShare args=%s", var_export($args, true)));
	}

	/**
	 * XXX
	 * org.openmediavault.services.nfs.shares.share
	 *
	 * @param string event
	 * @access public
	 */
	public function onDeleteNFSShare($args) {
        $this->debug(sprintf("onDeleteNFSShare args=%s", var_export($args, true)));
	}

	/**
	 * XXX
	 * org.openmediavault.services.nfs.shares.share
	 *
	 * @param string event
	 * @access public
	 */
	public function onUpdateNFSShare($args) {
        $this->debug(sprintf("onUpdateNFSShare args=%s", var_export($args, true)));
	}

	private function updatePools() {

	}
}

?>
