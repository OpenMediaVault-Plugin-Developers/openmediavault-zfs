<?php
/**
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @copyright Copyright (c) 2013-2026 openmediavault plugin developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
require_once("openmediavault/functions.inc");

// Log format produced by the ZED script all-omv-zfs-events.sh:
// 2026-03-27T10:00:00+00:00: tank pool.import
\OMV\System\LogFileSpec::registerSpecification("omv-zfs-events", [
    "filename" => "omv-zfs-events.log",
    "filepath" => "/var/log/omv-zfs-events.log",
    "regex"    => "/^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[^\s]*): (\S+) (.+)$/",
    "columns"  => [
        "ts"    => 1,
        "pool"  => 2,
        "class" => 3,
    ]
]);
