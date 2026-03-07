<?php
/*
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2014-2026 openmediavault plugin developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

namespace OMV\System\Filesystem\Backend;

class Zfs extends BackendAbstract
{
    public function __construct()
    {
        $this->type       = 'zfs';
        $this->properties = self::PROP_POSIX_ACL;
    }

    public function getImpl($args)
    {
        $object = new \OMV\System\Filesystem\FilesystemZfs($args);
        $object->setBackend($this);
        return $object;
    }

    public function isTypeOf($fsName)
    {
        $process = new \OMV\System\Process(
            sprintf('zfs list -H -o name -t filesystem %s', escapeshellarg($fsName)));
        try {
            $process->execute($out, $res);
            return ($res == 0);
        } catch (\Exception $e) {
            return false;
        }
    }

    public function enumerate()
    {
        // Get all ZFS filesystems, excluding root and boot mounts.
        $process = new \OMV\System\Process(
            "zfs list -p -H -o name,mountpoint -t filesystem | awk '\$2 != \"/\" && \$2 != \"/boot\" { print \$0 }'");
        try {
            $process->execute($out, $res);
        } catch (\Exception $e) {
            return [];
        }
        $result = [];
        foreach ($out as $line) {
            $parts = preg_split('/\t+/', $line);
            $name  = $parts[0];
            $result[$name] = [
                'devicefile' => $name,
                'uuid'       => '',
                'label'      => $name,
                'type'       => 'zfs',
            ];
        }
        return $result;
    }

    public function isBlkidEnumerated()
    {
        return false;
    }
}
