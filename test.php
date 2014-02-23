<?php
	require 'OMVModuleZPool.php';

	$c = new OMVModuleZPool(array('type' => 1, 'vdevs' => 2), array(), true);
	$c->addPool(array('/dev/sda', '/dev/sdb', '/dev/sdc', '/dev/sdd'), 'test',
				   array('log' => array('disks' => ['/dev/sde', '/dev/sdf'], 'mirror' => true),
				         'cache' => ['/dev/sdg'],
				         'spare' => ['/dev/sdh', '/dev/sdi'])
	);
	//var_dump($c);
?>
