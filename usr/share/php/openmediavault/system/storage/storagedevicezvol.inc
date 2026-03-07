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

namespace OMV\System\Storage;

/**
 * This class provides a simple interface to handle ZFS Zvol devices.
 * @ingroup api
 */
class StorageDeviceZvol extends StorageDevice
{
    public function getDescription()
    {
        $name = '';
        try {
            $process = new \OMV\System\Process(
                sprintf('/lib/udev/zvol_id %s', escapeshellarg($this->getDeviceFile())));
            $process->execute($out, $res);
            $name = $out[0] ?? '';
        } catch (\Exception $e) {}
        return sprintf(gettext('Zvol %s [%s, %s]'),
            $name, $this->getDeviceFile(), binary_format($this->getSize()));
    }
}
