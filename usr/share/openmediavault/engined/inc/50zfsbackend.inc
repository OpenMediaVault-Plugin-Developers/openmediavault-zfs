<?php
$mngr = \OMV\System\Storage\Backend\Manager::getInstance();
$mngr->registerBackend(new \OMV\System\Storage\Backend\Zvol());
$mngr = \OMV\System\Filesystem\Backend\Manager::getInstance();
$mngr->registerBackend(new \OMV\System\Filesystem\Backend\Zfs());
