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

// Log format produced by zfs-remote-unlock:
// 2026-03-12 10:32:01 zfs-remote-unlock: INFO: Fetching key for tank/data from https://...
\OMV\System\LogFileSpec::registerSpecification("zfs-remote-unlock", [
    "filename" => "zfs-remote-unlock.log",
    "filepath" => "/var/log/zfs-remote-unlock.log",
    "regex"    => "/^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) zfs-remote-unlock: (\w+): (.*)$/",
    "columns"  => [
        "ts"      => 1,
        "level"   => 2,
        "message" => 3,
    ]
]);
