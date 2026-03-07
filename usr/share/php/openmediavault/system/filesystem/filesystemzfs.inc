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

namespace OMV\System\Filesystem;

class FilesystemZfs implements FilesystemInterface, SharedFolderCandidateInterface
{
    public function __construct($fsName)
    {
        $this->deviceFile = $fsName;
        $this->label      = $fsName;
        $this->type       = 'zfs';
        $this->dataCached = false;
        $this->usage      = 'filesystem';
    }

    public function getPartitionType()
    {
        return $this->getPartitionScheme();
    }

    public function getPartitionScheme()
    {
        return null;
    }

    public function getUsage()
    {
        return $this->usage;
    }

    public function getPartitionEntry()
    {
        return null;
    }

    public function getMountPoint()
    {
        $cmd = sprintf('zfs get -H -p mountpoint %s 2>&1', escapeshellarg($this->deviceFile));
        try {
            $process = new \OMV\System\Process($cmd);
            $process->execute($out, $res);
            $parts = preg_split('/\t+/', $out[0] ?? '');
            return $parts[2] ?? false;
        } catch (\Exception $e) {
            return false;
        }
    }

    public function getStatistics()
    {
        $cmd = sprintf(
            'zfs get -H -p used,available,mountpoint %s 2>&1',
            escapeshellarg($this->deviceFile));
        try {
            $process = new \OMV\System\Process($cmd);
            $process->execute($out, $res);
        } catch (\Exception $e) {
            return false;
        }
        $props = [];
        foreach ($out as $line) {
            $parts = preg_split('/\t+/', $line);
            if (count($parts) >= 3) {
                $props[$parts[1]] = $parts[2];
            }
        }
        $used      = (int)($props['used'] ?? 0);
        $available = (int)($props['available'] ?? 0);
        $size      = $used + $available;
        return [
            'devicefile' => $this->deviceFile,
            'type'       => 'zfs',
            'blocks'     => $size / 1024,
            'size'       => $size,
            'used'       => $used,
            'available'  => $available,
            'percentage' => $size > 0 ? intval($used / $size * 100) : 0,
            'mountpoint' => $props['mountpoint'] ?? '',
        ];
    }

    public function exists()
    {
        $process = new \OMV\System\Process(
            sprintf('zfs list -H -t filesystem %s 2>&1', escapeshellarg($this->deviceFile)));
        try {
            $process->execute($out, $res);
            return ($res === 0);
        } catch (\Exception $e) {
            return false;
        }
    }

    public function getDeviceFile()
    {
        return $this->deviceFile;
    }

    public function getDeviceName()
    {
        return $this->deviceFile;
    }

    public function getDetails()
    {
        return '';
    }

    public function getCanonicalDeviceFile()
    {
        return $this->deviceFile;
    }

    public function getDeviceFileById()
    {
        return null;
    }

    public function hasDeviceFileById()
    {
        return false;
    }

    public function getDeviceFileByUuid()
    {
        return null;
    }

    public function hasDeviceFileByUuid()
    {
        return false;
    }

    public function getDeviceFileByLabel()
    {
        return null;
    }

    public function hasDeviceFileByLabel()
    {
        return false;
    }

    public function getPredictableDeviceFile()
    {
        return $this->deviceFile;
    }

    public function getPreferredDeviceFile()
    {
        return $this->deviceFile;
    }

    public function getDeviceFileSymlinks()
    {
        return [];
    }

    public function getDeviceFiles()
    {
        $poolDevices = $this->getPoolDeviceFiles();
        if (!empty($poolDevices)) {
            return $poolDevices;
        }
        return [$this->deviceFile];
    }

    public function getParentDeviceFile()
    {
        $poolDevices = $this->getPoolDeviceFiles();
        if (empty($poolDevices)) {
            return null;
        }
        $mngr = \OMV\System\Storage\Backend\Manager::getInstance();
        foreach ($poolDevices as $device) {
            $backend = $mngr->getBackend($device);
            if ($backend !== null) {
                $parentDeviceFile = $backend->baseDeviceFile($device);
                if (is_devicefile($parentDeviceFile)) {
                    return realpath($parentDeviceFile);
                }
            }
            if (is_devicefile($device)) {
                return realpath($device);
            }
        }
        return null;
    }

    public function hasFstabSupport()
    {
        return false;
    }

    public function hasMultipleDevices()
    {
        $devs = $this->getDeviceFiles();
        if (false === is_array($devs)) {
            return false;
        }
        return (1 < count($devs));
    }

    public function hasUuid()
    {
        return false;
    }

    public function getUuid()
    {
        return null;
    }

    public function hasLabel()
    {
        $label = $this->getLabel();
        if (false === $label) {
            return false;
        }
        return !empty($label);
    }

    public function getLabel()
    {
        return $this->label;
    }

    public function getType()
    {
        return $this->type;
    }

    public function getSize()
    {
        return 0;
    }

    final public function setBackend(Backend\BackendAbstract $backend)
    {
        $this->backend = $backend;
    }

    final public function getBackend()
    {
        return $this->backend;
    }

    public function getDescription()
    {
        $statistics = $this->getStatistics();
        return sprintf(
            '%s [%s (%d%%) used, %s available]',
            $this->getDeviceFile(),
            binary_format($statistics['used']),
            $statistics['percentage'],
            binary_format($statistics['available']));
    }

    public function isMounted()
    {
        $cmd = sprintf('zfs get -H -p mounted %s 2>&1', escapeshellarg($this->deviceFile));
        try {
            $process = new \OMV\System\Process($cmd);
            $process->execute($out, $res);
            $parts = preg_split('/\t+/', $out[0] ?? '');
            return (($parts[2] ?? '') === 'yes');
        } catch (\Exception $e) {
            return false;
        }
    }

    public function mount($options = '')
    {
        try {
            $process = new \OMV\System\Process(
                sprintf('zfs mount %s 2>&1', escapeshellarg($this->deviceFile)));
            $process->execute($out, $res);
            return true;
        } catch (\OMV\ExecException $e) {
            return false;
        }
    }

    public function umount($force = false, $lazy = false, $directory = false)
    {
        try {
            $process = new \OMV\System\Process(
                sprintf('zfs unmount %s 2>&1', escapeshellarg($this->deviceFile)));
            $process->execute($out, $res);
            return true;
        } catch (\OMV\ExecException $e) {
            return false;
        }
    }

    public function grow()
    {
    }

    public function shrink()
    {
    }

    public function remove()
    {
        try {
            $process = new \OMV\System\Process(
                sprintf('zfs destroy %s 2>&1', escapeshellarg($this->deviceFile)));
            $process->execute($out, $res);
            return true;
        } catch (\OMV\ExecException $e) {
            return false;
        }
    }

    /**
     * Get all underlying device files from the pool that contains this filesystem.
     */
    private function getPoolDeviceFiles(): array
    {
        $parts    = explode('/', $this->deviceFile);
        $poolName = $parts[0];
        try {
            $cmd = sprintf('zpool status -P %s 2>&1', escapeshellarg($poolName));
            $process = new \OMV\System\Process($cmd);
            $process->execute($out, $res);
            $devices  = [];
            $inConfig = false;
            foreach ($out as $line) {
                if (preg_match('/^\s*NAME\s+STATE/', $line)) {
                    $inConfig = true;
                    continue;
                }
                if (!$inConfig) {
                    continue;
                }
                if (trim($line) === '') {
                    break;
                }
                if (preg_match('/^\s{4,}(\/\S+)\s+\S+/', $line, $m)) {
                    $devices[] = $m[1];
                }
            }
            return array_values(array_filter($devices, 'is_devicefile'));
        } catch (\Throwable $e) {
            return [];
        }
    }
}
