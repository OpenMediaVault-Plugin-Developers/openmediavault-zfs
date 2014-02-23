<?php
/*
 * OMVModulePoolAction.php
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


class OMVModulePoolAction {
	const OMVModulePoolAction_TYPE_CREATE	= 0;
	const OMVModulePoolAction_TYPE_ADD		= 1;

	/**
	 * Return pool action
	 * @param type An OMVModulePoolAction.
	 * @return action
	 * @throws Exception
	 */
	public static function getAction($action) {
		if ($action === self::OMVModulePoolAction_TYPE_CREATE) {
			return 'create';
		} else if ($action === self::OMVModulePoolAction_TYPE_ADD) {
			return 'add';
		} else {
			throw new Exception("$action: Unknown OMVModulePoolType");
		}
	}

	/**
	 * Return OMVModulePoolAction as string.
	 * @param type An OMVModulePoolAction.
	 * @return type as string.
	 * @throws Exception.
	 */
	public static function toString($type) {
		if ($type === self::OMVModulePoolAction_TYPE_CREATE) {
			return 'OMVModulePoolAction_TYPE_CREATE';
		} else if ($type === self::OMVModulePoolAction_TYPE_ADD) {
			return 'OMVModulePoolAction_TYPE_ADD';
		} else {
			throw new Exception("$type: Unknown OMVModulePoolType");
		}
	}
}
