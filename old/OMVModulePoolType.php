<?php
/*
 * OMVPoolModuleType.php
 *
 * Copyright 2013 Michael Rasmussen <mir@datanom.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
 * @class OMVModulePoolType.
 */
class OMVModulePoolType {
	const OMVModulePoolType_TYPE_NONE	= 0;
	const OMVModulePoolType_TYPE_MIRROR	= 1;
	const OMVModulePoolType_TYPE_RAIDZ1	= 2;
	const OMVModulePoolType_TYPE_RAIDZ2	= 3;
	const OMVModulePoolType_TYPE_RAIDZ3	= 4;

	/**
	 * Return OMVModulePoolType as string.
	 * @param type An OMVModulePoolType.
	 * @return type as string.
	 * @throws Exception.
	 */
	public static function toString($type) {
		if ($type === self::OMVModulePoolType_TYPE_NONE) {
			return 'OMVModulePoolType_TYPE_NONE';
		} else if ($type === self::OMVModulePoolType_TYPE_MIRROR) {
			return 'OMVModulePoolType_TYPE_MIRROR';
		} else if ($type === self::OMVModulePoolType_TYPE_RAIDZ1) {
			return 'OMVModulePoolType_TYPE_RAIDZ1';
		} else if ($type === self::OMVModulePoolType_TYPE_RAIDZ2) {
			return 'OMVModulePoolType_TYPE_RAIDZ2';
		} else if ($type === self::OMVModulePoolType_TYPE_RAIDZ3) {
			return 'OMVModulePoolType_TYPE_RAIDZ3';
		} else {
			throw new Exception("$type: Unknown OMVModulePoolType");
		}
	}
}
