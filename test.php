<?php
	require 'OMVModuleZPool.php';

	$c = new OMVModuleZPool(array('type' => 1, 'vdevs' => 2), true);
	$c->createPool(array('/dev/sda', '/dev/sdb', '/dev/sdc', '/dev/sdd'));
?>
