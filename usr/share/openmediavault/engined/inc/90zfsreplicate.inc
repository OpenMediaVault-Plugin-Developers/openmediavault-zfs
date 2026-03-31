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

// Log format produced by omv-zfs-replicate:
// 2026-03-25T10:00:00+00:00: [tank/data -> backup/data] Creating snapshot tank/data@replicate-20260325-100000
\OMV\System\LogFileSpec::registerSpecification("omv-zfs-replicate", [
    "filename" => "omv-zfs-replicate.log",
    "filepath" => "/var/log/omv-zfs-replicate.log",
    "regex"    => "/^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[^\s]*): (\[[^\]]+\]) (.*)$/",
    "columns"  => [
        "ts"      => 1,
        "job"     => 2,
        "message" => 3,
    ]
]);
