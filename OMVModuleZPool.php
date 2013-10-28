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

require 'OMVModulePoolType.php';

class OMVModuleZPool {

	private $params;
	private $log;
	private $mirror_log;
	private $cache;
	private $spare;

	/**
	 * Constructor of class OMVModuleZPool.
	 * @param params Array which can contain all or a subset of
	 *   the following fields:
	 *   \em atime Either true or false. <b>Default is true</b>.
	 *   \em compress Either true or false. <b>Default is false</b>.
	 *   \em dedub Either true or false. <b>Default is false</b>.
	 *   \em name Name for pool
	 *   \em size Size of pool in either MB or GB. 100M or 100G.
	 *   \em sync Either true or false. <b>Default is false</b>.
	 *   \em type See OMVModulePoolType.
	 *   \em vdevs Number of vdevs in pool. <b>Default is 1</b>.
	 * @param debug Print debug information to STDERR.
	 * @throws Exception.
	 */
	public function __construct(array $params = array(), $debug = false) {
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

		foreach ($params as $key => $value) {
			if (array_key_exists($key, $this->params)) {
				$this->validate($key, $value);
				$this->params[$key] = $value;
			} else {
				throw new Exception("$key: Not valid parameter");
			}
		}

		if ($debug) {
			fprintf(STDERR, "OMVModuleZPool instantiate with the following params\n");
			foreach ($this->params as $key => $value) {
				$param = ($key == 'type') ? OMVModulePoolType::toString($value) : $value;
				fprintf(STDERR, "  %-8s => %s\n", $key, $param);
			}
		}
	}

	/**
	 * Create pool.
	 * @param disks Array of disks. Format: /dev/sdx or /dev/vdx.
	 * @param options Array which can contain all or a subset of
	 *   the following fields:
	 *   \em log Array which contains the following fields:
	 *      \em disks Array of disks. Format: /dev/sdx or /dev/vdx.
	 *      \em mirror Either true or false. <b>Default is false</b>.
	 *   \em cache Disk to use for cache. <b>Default use internal</b>.
	 *   \em spare Array of disks. Format: /dev/sdx or /dev/vdx.
	 * @throws Exception.
	 */
	public function createPool(array $disks, array $options = array()) {
		$disk_num = count($disks);
		if ($disk_num < 1) {
			throw new Exception("Pool must have at least 1 disk");
		};
		switch ($this->params['type']) {
			case OMVModulePoolType::OMVModulePoolType_TYPE_NONE:
				print "Create basic pool\n";
				if ($disk_num % $this->params['vdevs']) {
					throw new Exception("$disk_num disk(s) != ".$this->params['vdevs']." vdev(s)");
				}
				foreach ($options as $key => $value) {
					$this->validate($key, $value, OMVPoolType::OMV_TYPE_NONE);
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_MIRROR:
				print "Create mirrored pool\n";
				if ($disk_num / $this->params['vdevs'] < 2 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk(s) cannot be evenly distributed to ".
					   $this->params['vdevs']." vdev(s) and form a proper mirror");
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ1:
				print "Create raidz1 pool\n";
				if ($disk_num / $this->params['vdevs'] < 3 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 3 disks per vdev for raidz1");
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ2:
				print "Create raidz2 pool\n";
				if ($disk_num / $this->params['vdevs'] < 5 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 5 disks per vdev for raidz2");
				}
				break;
			case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ3:
				print "Create raidz3 pool\n";
				if ($disk_num / $this->params['vdevs'] < 8 ||
				   ($disk_num / $this->params['vdevs']) % $this->params['vdevs']) {
					throw new Exception("$disk_num disk and ".$this->params['vdevs'].
					   " vdev(s) cannot provide mininum 8 disks per vdev for raidz3");
				}
				break;
		}
	}

	private function validate($key, $value, $type = -1) {
		if ($type > -1) {
			switch ($type) {
				case OMVModulePoolType::OMVModulePoolType_TYPE_NONE:
					print "Create basic pool\n";
					break;
				case OMVModulePoolType::OMVModulePoolType_TYPE_MIRROR:
					print "Create mirrored pool\n";
					break;
				case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ1:
					print "Create raidz1 pool\n";
					break;
				case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ2:
					print "Create raidz2 pool\n";
					break;
				case OMVModulePoolType::OMVModulePoolType_TYPE_RAIDZ3:
					print "Create raidz3 pool\n";
					break;
				default:
					throw new Exception("$type: Unknown OMVModulePoolType");
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
