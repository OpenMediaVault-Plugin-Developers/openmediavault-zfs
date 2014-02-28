<?php

/**
 * OMVModuleZFSVdevType class
 *
 * @author    Michael Rasmussen
 * @version   0.1
 * @copyright Michael Rasmussen <mir@datanom.net>
 * @abstract
 */
abstract class OMVModuleZFSVdevType {

    /**
     * @var  OMVMODULEZFSPLAIN
     * @access public
     */
	const OMVMODULEZFSPLAIN = 0;

    /**
     * @var  OMVMODULEZFSMIRROR
     * @access public
     */
	const OMVMODULEZFSMIRROR = 1;

    /**
     * @var  OMVMODULEZFSRAIDZ1
     * @access public
     */
	const OMVMODULEZFSRAIDZ1 = 2;

    /**
     * @var  OMVMODULEZFSRAIDZ2
     * @access public
     */
	const OMVMODULEZFSRAIDZ2 = 3;

    /**
     * @var  OMVMODULEZFSRAIDZ3
     * @access public
     */
	const OMVMODULEZFSRAIDZ3 = 4;

}

?>
