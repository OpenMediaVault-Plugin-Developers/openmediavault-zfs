<?php

/**
 * XXX detailed description
 *
 * @author    XXX
 * @version   XXX
 * @copyright XXX
 */
class Vdev {
    // Attributes
    /**
     * XXX
     *
     * @var    list<Disk> $disks
     * @access private
     */
    protected $_disks;

    // Associations
    // Operations
    /**
     * XXX
     *
     * @param  Disk $disk XXX
     * @return void XXX
     * @access public
     */
    public function addDisk($disk) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @param  Disk $disk XXX
     * @return void XXX
     * @access public
     */
    public abstract function removeDisk($disk) {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

    /**
     * XXX
     *
     * @return list<Disk> XXX
     * @access public
     */
    public function getDisks() {
        trigger_error('Not Implemented!', E_USER_WARNING);
    }

}

?>
