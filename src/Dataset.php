<?php

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
     * @var    array $features
     * @access private
     */
    private $_features;

    // Associations
    // Operations
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
     * @return array XXX
     * @access public
     */
    public function getFeatures() {
        trigger_error('Not Implemented!', E_USER_WARNING);
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
