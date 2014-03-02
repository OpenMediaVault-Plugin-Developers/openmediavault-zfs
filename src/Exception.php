<?php

/**
 * OMVModuleZFSException class
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 * @abstract
 */
class OMVModuleZFSException extends Exception {

    /**
     * XXX
     *
     * @param  Disk $disk XXX
     * @return void XXX
     * @access public
     */
    public function __construct($message = "", $code = 0, Exception $previous = NULL) {
        parent::__construct($message, $code, $previous);
    }

}

?>
