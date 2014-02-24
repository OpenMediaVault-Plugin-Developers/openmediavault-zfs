<?php
/*
 * OMVZModulePool.php
 *
 * Copyright 2013 Michael Rasmussen <mir@datanom.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/**
 * @class OMVModuleZPool.
 */

require ('OMVModulePoolType.php');
require ('OMVModulePoolAction.php');

class OMVModuleZPool {

	private $params;
	private $log;
	private $mirror_log;
	private $cache;
	private $spare;
	private $alloc;
	private $status; // one of null, ok, degraded, faulted, scrub, resilver
	private $debug;

	/**
	 * Constructor of class OMVModuleZPool.
	 * @param params Array which can contain all or a subset of
	 *   the following fields:
	 * 	<ul>
	 *   <li> atime Either true or false. <b>Default is true</b>.
	 *   <li> compress Either true or false. <b>Default is false</b>.
	 *   <li> dedub Either true or false. <b>Default is false</b>.
	 *   <li> name Name for pool
	 *   <li> size Size of pool in either MB or GB. 100M or 100G.
	 *   <li> sync Either true or false. <b>Default is false</b>.
	 *   <li> type See OMVModulePoolType.
	 *   <li> vdevs Number of vdevs in pool. <b>Default is 1</b>.
	 * 	</ul>
	 * @param options Array which can contain all or a subset of
	 *   the following fields:
	 *   <ul>
	 *   <li> log Array which contains the following fields:
	 * 		<ul>
	 *      <li> disks Array of disks. Format: /dev/disk/by-id/.....
	 *      <li> mirror Either true or false. <b>Default is false</b>.
	 *		</ul>
	 *   <li> cache Array which contains the following fields:
	 * 		<ul>
	 * 		<li> disks Array of disks. Format: /dev/disk/by-id/.... <b>Default use internal</b>.
	 * 		</ul>
	 *   <li> spare Array of disks. Format: /dev/disk/by-id/...
	 * 	</ul>
	 * @param debug Print debug information to STDERR.
	 * @throws Exception.
	 */
	public function __construct(array $params = array(), array $options = array(), $debug = false) {
		$this->params = array(
			'atime'		=> 'on',
			'compress'	=> 'off',
			'dedub'		=> 'off',
			'name'		=> '',
			'size'		=> '0M',
			'sync'		=> 'off',
			'type'		=> OMVModulePoolType::OMVModulePoolType_TYPE_NONE,
			'vdevs'		=> 1
		);
		$this->debug = $debug;

		foreach ($params as $key => $value) {
			if (array_key_exists($key, $this->params)) {
				$this->validate($key, $value);
				$this->params[$key] = $value;
			} else {
				throw new Exception("$key: Not valid parameter");
			}
		}

		foreach ($options as $key => $value) {
			$this->validate($key, $value, true);
		}

		$this->status = null;

		if ($this->debug) {
			fprintf(STDERR, "OMVModuleZPool instantiate with the following params\n");
			foreach ($this->params as $key => $value) {
				$param = ($key == 'type') ? OMVModulePoolType::toString($value) : $value;
				fprintf(STDERR, "  %-8s => %s\n", $key, $param);
			}
		}
	}

	/**
	 * Create pool.
	 * @param disks Array of disks. Format: /dev/disk/by-id/...
	 * @param name Name for the pool.
	 * @param options Array which can contain all or a subset of
	 *   the following fields:
	 * 	<ul>
	 *   <li> log Array which contains the following fields:
	 * 		<ul>
	 *      <li> disks Array of disks. Format: /dev/disk/by-id/...
	 *      <li> mirror Either true or false. <b>Default is false</b>.
	 * 		</ul>
	 *   <li> cache Array which contains the following fields:
	 * 		<ul>
	 * 		<li> disks Array of disks. Format: /dev/disk/by-id/... <b>Default use internal</b>.
	 * 		</ul>
	 *   <li> spare Array of disks. Format: /dev/disk/by-id/...
	 * 	</ul>
	 * @throws Exception.
	 */
	public function createPool(array $disks, $name = null, array $options = array()) {
		return $this->updatePool(OMVModulePoolAction::OMVModulePoolAction_TYPE_CREATE,
								 $disks, count($disks), $name, $options);
	}

	/**
	 * Add pool.
	 * @param disks Array of disks. Format: /dev/disk/by-id/...
	 * @param name Name for the pool.
	 * @param options Array which can contain all or a subset of
	 *   the following fields:
	 * 	<ul>
	 *   <li> log Array which contains the following fields:
	 * 		<ul>
	 *      <li> disks Array of disks. Format: /dev/disk/by-id/...
	 *      <li> mirror Either true or false. <b>Default is false</b>.
	 * 		</ul>
	 *   <li> cache Array which contains the following fields:
	 * 		<ul>
	 * 		<li> disks Array of disks. Format: /dev/disk/by-id/... <b>Default use internal</b>.
	 * 		</ul>
	 *   <li> spare Array of disks. Format: /dev/disk/by-id/...
	 * 	</ul>
	 * @throws Exception.
	 */
	public function addPool(array $disks, $name = null, array $options = array()) {
		$this->initPool($name);
		return $this->updatePool(OMVModulePoolAction::OMVModulePoolAction_TYPE_ADD,
								 $disks, count($disks), $name, $options);
	}

	public function removeDevice(array $disks, $name = null) {
	}

	public function destroyPool($name = null) {
	}

	public function scrubPool($name = null) {
	}

	public function exportPool($name = null){
	}

	public function importPool($name = null){
	}

	/*
	 * public function attachDevice($extingDisk, $newDisk, $name = null)
	 * public function detachDevice($disk, $name = null)
	 * public function onlineDevice($disk, $name = null)
	 * public function offlineDevice($disk, $name = null)
	 * public function replaceDevice($extingDisk, $newDisk, $name = null)
	 * public function upgradePool($name = null)
	 */

	private function initPool($name = null) {
		// Replace with OMVUtil::exec
		$this->validateName($name);
	}

	private function validateName($name = null) {
		$oldname = null;
		if ($name != null) {
			$oldname = $this->params['name'];
			$this->params['name'] = $name;
		}
		if (! $this->params['name'] || ! preg_match('/^\w+$/', $this->params['name'])) {
			if ($oldname) {
				$this->params['name'] = $oldname;
			}
			throw new Exception("Name of pool cannot be null or empty");
		}
	}

	/**
	 * Update pool.
	 * @param action Action to be taken. create or add.
	 * @param disks Array of disks. Format: /dev/disk/by-id/...
	 * @param disk_num Number of disk(s) in the pool.
	 * @param name Name for the pool.
	 * @param options Array which can contain all or a subset of
	 *   the following fields:
	 * 	<ul>
	 *   <li> log Array which contains the following fields:
	 * 		<ul>
	 *      <li> disks Array of disks. Format: /dev/disk/by-id/...
	 *      <li> mirror Either true or false. <b>Default is false</b>.
	 * 		</ul>
	 *   <li> cache Array which contains the following fields:
	 * 		<ul>
	 * 		<li> disks Array of disks. Format: /dev/disk/by-id/... <b>Default use internal</b>.
	 * 		</ul>
	 *   <li> spare Array of disks. Format: /dev/disk/by-id/...
	 * 	</ul>
	 * @throws Exception.
	 */
	private function updatePool($action, array $disks, $disk_num, $name = null, array $options = array()) {
		OMVModulePoolAction::toString($action);
		$cmd = 'zpool ' . OMVModulePoolAction::getAction($action);
		if ($disk_num < 1) {
			throw new Exception("Pool must have at least 1 disk");
		};
		$this->validateName($name);
		foreach ($options as $key => $value) {
			$this->validate($key, $value, true);
		}
		switch ($action) {
			case OMVModulePoolAction::OMVModulePoolAction_TYPE_CREATE:
				$cmd .= ' ' . $this->params['name'];
				break;
			case OMVModulePoolAction::OMVModulePoolAction_TYPE_ADD:
				$cmd .= ' ' . $this->params['name'];
				break;
		}
		switch ($this->params['type']) {
			case OMVModulePoolType::OMVModulePoolType_TYPE_NONE:
				print "Create basic pool\n";
				if ($this->params['vdevs'] != 1) {
					throw new Exception("A basic zpool can only have 1 vdev");
				}
				$vdev = '';
				foreach ($disks as $disk) {
					$vdev .= ($vdev) ? " $disk" : "$disk";
				}
				$cmd .= " $vdev";
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_MIRROR:
				print "Create mirrored pool\n";
				if ($disk_num / $this->params['vdevs'] < 2 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk(s) cannot be evenly distributed to ".
					   $this->params['vdevs']." vdev(s) and form a proper stripe or mirror");
				}
				$vdevs = array();
				$disk_sum = $disk_num / $this->params['vdevs'];
				$s = '';
				for ($i = 1, $num = 0; $i <= $disk_num; $i++) {
					$s .= ($s) ? " ${disks[$i-1]}" : "${disks[$i-1]}";
					if ($i % $disk_sum == 0) {
						$vdevs[$num++] = $s;
						$s = '';
					}
				}
				foreach ($vdevs as $vdev) {
					$cmd .= " mirror $vdev";
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ1:
				print "Create raidz1 pool\n";
				if ($disk_num / $this->params['vdevs'] < 3 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 3 disks per vdev for raidz1");
				}
				$vdevs = array();
				$disk_sum = $disk_num / $this->params['vdevs'];
				$s = '';
				for ($i = 1, $num = 0; $i <= $disk_num; $i++) {
					$s .= ($s) ? " ${disks[$i-1]}" : "${disks[$i-1]}";
					if ($i % $disk_sum == 0) {
						$vdevs[$num++] = $s;
						$s = '';
					}
				}
				foreach ($vdevs as $vdev) {
					$cmd .= " raidz1 $vdev";
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ2:
				print "Create raidz2 pool\n";
				if ($disk_num / $this->params['vdevs'] < 4 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 4 disks per vdev for raidz2");
				}
				$vdevs = array();
				$disk_sum = $disk_num / $this->params['vdevs'];
				$s = '';
				for ($i = 1, $num = 0; $i <= $disk_num; $i++) {
					$s .= ($s) ? " ${disks[$i-1]}" : "${disks[$i-1]}";
					if ($i % $disk_sum == 0) {
						$vdevs[$num++] = $s;
						$s = '';
					}
				}
				foreach ($vdevs as $vdev) {
					$cmd .= " raidz2 $vdev";
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ3:
				print "Create raidz3 pool\n";
				if ($disk_num / $this->params['vdevs'] < 5 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 5 disks per vdev for raidz3");
				}
				$vdevs = array();
				$disk_sum = $disk_num / $this->params['vdevs'];
				$s = '';
				for ($i = 1, $num = 0; $i <= $disk_num; $i++) {
					$s .= ($s) ? " ${disks[$i-1]}" : "${disks[$i-1]}";
					if ($i % $disk_sum == 0) {
						$vdevs[$num++] = $s;
						$s = '';
					}
				}
				foreach ($vdevs as $vdev) {
					$cmd .= " raidz3 $vdev";
				}
				break;
		}
		foreach ($options as $key => $value) {
			switch ($key) {
				case 'log':
					$cmd .= " log";
					if ($value['mirror'] === true) {
						$cmd .= " mirror";
					}
					foreach ($value['disks'] as $disk) {
						$cmd .= " $disk";
					}
					break;
				case 'cache':
					$cmd .= " cache";
					foreach ($value as $disk) {
						$cmd .= " $disk";
					}
					break;
				case 'spare':
					$cmd .= " spare";
					foreach ($value as $disk) {
						$cmd .= " $disk";
					}
					break;
			}
		}
		if ($this->debug) {
			fprintf(STDERR, "%s\n", $cmd);
		}

		return $cmd;
	}

	/**
	 * Validate options.
	 * @param key Option to validate.
	 * @param value Value for option to validate.
	 * @param type See OMVModulePoolType.
	 * @throws Exception if option is invalid.
	 */
	private function validate($key, $value, $options = false) {
		if ($options) {
			switch ($key) {
				case 'log':
					if (! is_array($value) || ! array_key_exists('mirror', $value) ||
						! array_key_exists('disks', $value)) {
						throw new Exception("log option missing disks array or mirror value");
					}
					if ($value['mirror'] === true && count($value['disks']) != 2) {
						throw new Exception("Two disks is required to form a log mirror");
					}
					$this->log = $value['disks'];
					$this->mirror_log = $value['mirror'];
					break;
				case 'cache':
					if (! is_array($value)) {
						throw new Exception("log option missing disks array or mirror value");
					}
					$this->cache = $value;
					break;
				case 'spare':
					if (! is_array($value)) {
						throw new Exception("log option missing disks array or mirror value");
					}
					$this->spare = $value;
					break;
				default:
					throw new Exception("$key: Not valid option");
			}
		} else {
			switch ($key) {
				case 'atime':
				case 'compress':
				case 'dedub':
				case 'sync':
					if ($value != 'on' && $value != 'off') {
						throw new Exception("$key: Value must be 'on' or 'off' found '$value'");
					}
					break;
				case 'size':
					if (! preg_match('/^\d+(M|G)$/', $value)) {
						throw new Exception("$key: Value must have format '100M' or '100G' found '$value'");
					}
					break;
				case 'type':
					if (OMVModulePoolType::OMVModulePoolType_TYPE_NONE > $value ||
						OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ3 < $value) {
						throw new Exception("$key: Value must be type of OMVPoolType found '$value'");
					}
					break;
				case 'vdevs':
					if (! preg_match('/^\d+$/', $value) || $value < 1) {
						throw new Exception("$key: Value must be positive int found '$value'");
					}
			}
		}
	}
}
