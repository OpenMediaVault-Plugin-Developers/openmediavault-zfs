<?php
require_once 'Vdev.php';
require_once 'Snapshot.php';
require_once 'Dataset.php';
require_once 'Zvol.php';

/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class OMVModuleZFSZpool {
    // Attributes
    /**
     * XXX
     *
     * @var    string $name
     * @access private
     */
    private $_name;

    /**
     * XXX
     *
     * @var    list<Vdev> $vdevs
     * @access private
     */
    private $_vdevs;

    /**
     * XXX
     *
     * @var    list<Disk> $spare
     * @access private
     */
    private $_spare;

    /**
     * XXX
     *
     * @var    Log $log
     * @access private
     */
    private $_log;

    /**
     * XXX
     *
     * @var    Cache $cache
     * @access private
     */
    private $_cache;

    /**
     * XXX
     *
     * @var    int $size
     * @access private
     */
    private $_size;

    /**
     * XXX
     *
     * @var    string $mountPoint
     * @access private
     */
    private $_mountPoint;

    /**
     * XXX
     *
     * @var    list<Feature> $features
     * @access private
     */
    private $_features;

    // Associations
    /**
     * XXX
     *
     * @var    Snapshot $unnamed
     * @access private
     * @accociation Snapshot to unnamed
     */
    #var $unnamed;

    /**
     * XXX
     *
     * @var    Dataset $unnamed
     * @access private
     * @accociation Dataset to unnamed
     */
    #var $unnamed;

    /**
     * XXX
     *
     * @var    Zvol $unnamed
     * @access private
     * @accociation Zvol to unnamed
     */
    #var $unnamed;

    /**
     * XXX
     *
     * @var    Vdev $unnamed
     * @access private
     * @accociation Vdev to unnamed
     */
    #var $unnamed;

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
     * @return list<Vdev> XXX
     * @access public
     */
    public function getVdevs() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Vdev $vdev XXX
     * @return void XXX
     * @access public
     */
    public function addVdev($vdev) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Vdev $vdev XXX
     * @return void XXX
     * @access public
     */
    public function removeVdev($vdev) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Cache $cache XXX
     * @return void XXX
     * @access public
     */
    public function addCache($cache) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void XXX
     * @access public
     */
    public function removeCache() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return Cache XXX
     * @access public
     */
    public function getCache() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Log $log XXX
     * @return void XXX
     * @access public
     */
    public function addLog($log) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void XXX
     * @access public
     */
    public function removeLog() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return Log XXX
     * @access public
     */
    public function getLog() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Disk $spare XXX
     * @return void XXX
     * @access public
     */
    public function addSpare($spare) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Disk $spare XXX
     * @return void XXX
     * @access public
     */
    public function removeSpare($spare) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return list<Disk> XXX
     * @access public
     */
    public function getSpares() {
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
     * @param  list<Feature> $features XXX
     * @return void XXX
     * @access public
     */
    public function setFeatures($features) {
        trigger_error('Not Implemented!', E_USER_WARNING);
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
     * @return void XXX
     * @access public
     */
    public function export() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  string $name XXX
     * @return void XXX
     * @access public
     */
    public function import($name) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return void XXX
     * @access public
     */
    public function scrub() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return string XXX
     * @access public
     */
    public function status() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

	/**
	 * XXX
	 * org.openmediavault.module.service.<servicename>.start
	 * org.openmediavault.module.service.<servicename>.stop
	 * org.openmediavault.module.service.<servicename>.applyconfig
	 *
	 * @param string event
	 * @access public
	 */
	public function onNotify($event) {
        trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @access public
	 */
	public function applyConfig() {
        trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @access public
	 */
	public function stopService() {
        trigger_error('Not Implemented!', E_USER_WARNING);
	}

	/**
	 * XXX
	 *
	 * @access public
	 */
	public function startService() {
        trigger_error('Not Implemented!', E_USER_WARNING);
	}
}

?>
