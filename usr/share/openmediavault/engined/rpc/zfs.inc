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

class OMVRpcServiceZFS extends \OMV\Rpc\ServiceAbstract
{
    public function getName()
    {
        return 'zfs';
    }

    public function initialize()
    {
        $this->registerMethod('doDiscover');
        $this->registerMethod('doDiscoverBg');
        $this->registerMethod('addPool');
        $this->registerMethod('importPool');
        $this->registerMethod('exportPool');
        $this->registerMethod('listCompressionTypes');
        $this->registerMethod('listPools');
        $this->registerMethod('listPoolsBg');
        $this->registerMethod('listPoolsOnly');
        $this->registerMethod('listPoolsOnlyBg');
        $this->registerMethod('listDatasets');
        $this->registerMethod('listDatasetsBg');
        $this->registerMethod('getAllSnapshots');
        $this->registerMethod('getAllSnapshotsBg');
        $this->registerMethod('addObject');
        $this->registerMethod('deleteObject');
        $this->registerMethod('deleteObjectBg');
        $this->registerMethod('deleteSnapshotRange');
        $this->registerMethod('deleteSnapshotRangeBg');
        $this->registerMethod('getProperties');
        $this->registerMethod('setProperties');
        $this->registerMethod('getObjectDetails');
        $this->registerMethod('expandPool');
        $this->registerMethod('getPoolDevices');
        $this->registerMethod('getAttachAnchors');
        $this->registerMethod('getTopLevelVdevs');
        $this->registerMethod('deviceOffline');
        $this->registerMethod('deviceOnline');
        $this->registerMethod('deviceDetach');
        $this->registerMethod('deviceReplace');
        $this->registerMethod('deviceAttach');
        $this->registerMethod('removeVdev');
        $this->registerMethod('cancelVdevRemoval');
        $this->registerMethod('getVdevRemovalStatus');
        $this->registerMethod('scrubPool');
        $this->registerMethod('stopScrub');
        $this->registerMethod('pauseScrub');
        $this->registerMethod('getScrubStatus');
        $this->registerMethod('renameObject');
        $this->registerMethod('cloneDataset');
        $this->registerMethod('promoteDataset');
        $this->registerMethod('getPoolHistory');
        $this->registerMethod('getPoolEvents');
        $this->registerMethod('quickSnapshot');
        $this->registerMethod('getEmptyCandidates');
        $this->registerMethod('rollbackSnapshot');
        $this->registerMethod('getStats');
        $this->registerMethod('getPoolHealth');
        $this->registerMethod('getPoolStatus');
        $this->registerMethod('getArcStats');
        $this->registerMethod('getDatasetNames');
        $this->registerMethod('getPoolNames');
        $this->registerMethod('getSnapshotJobList');
        $this->registerMethod('getSnapshotJob');
        $this->registerMethod('setSnapshotJob');
        $this->registerMethod('deleteSnapshotJob');
        $this->registerMethod('runSnapshotJob');
        $this->registerMethod('runSnapshotJobBg');
        $this->registerMethod('getScrubJobList');
        $this->registerMethod('getScrubJob');
        $this->registerMethod('setScrubJob');
        $this->registerMethod('deleteScrubJob');
        $this->registerMethod('runScrubJob');
        $this->registerMethod('runScrubJobBg');
        $this->registerMethod('fixPackages');
        $this->registerMethod('enableEncryption');
        $this->registerMethod('loadEncryptionKey');
        $this->registerMethod('unloadEncryptionKey');
        $this->registerMethod('changeEncryptionKey');
        $this->registerMethod('getEncryptionStatus');
        $this->registerMethod('setAutoUnlock');
        $this->registerMethod('encryptRemoteKey');
        $this->registerMethod('removeAutoUnlock');
        $this->registerMethod('clearPool');
        $this->registerMethod('trimPool');
        $this->registerMethod('upgradePool');
        $this->registerMethod('addVdev');
        $this->registerMethod('getSnapshotDiff');
    }

    public function doDiscover($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $addMissing  = $params['addMissing']  ?? true;
        $deleteStale = $params['deleteStale'] ?? false;
        self::fixOMVMntEnt($context, $addMissing, $deleteStale);
    }

    public function doDiscoverBg($params, $context)
    {
        return $this->callMethodBg('doDiscover', $params, $context);
    }

    public function getStats($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $arcstats = file('/proc/spl/kstat/zfs/arcstats');
        $hits = 0;
        $misses = 0;
        $size = 0;
        $cMax = 0;
        foreach ($arcstats as $line) {
            $parts = preg_split('/\s+/', $line);
            $key = $parts[0] ?? '';
            if ($key === 'hits')   { $hits   = (int)$parts[2]; }
            if ($key === 'misses') { $misses = (int)$parts[2]; }
            if ($key === 'size')   { $size   = (int)$parts[2]; }
            if ($key === 'c_max')  { $cMax   = (int)$parts[2]; }
        }
        if ($hits > 0 || $misses > 0) {
            return [
                'hits'       => $hits,
                'misses'     => $misses,
                'hitsMisses' => $hits + $misses,
                'ratio'      => ($hits / ($hits + $misses)) * 100,
                'size'       => $size,
                'c_max'      => $cMax,
            ];
        }
        return ['hits' => 0, 'misses' => 0, 'hitsMisses' => 0, 'ratio' => 0, 'size' => 0, 'c_max' => $cMax];
    }

    public function getPoolHealth($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $result = [];
        if (!self::zfsTryExec('zpool list -H -o name,health 2>&1', $out)) {
            return $result;
        }
        foreach ($out as $line) {
            $parts = preg_split('/\t/', trim($line));
            if (count($parts) < 2) {
                continue;
            }
            $result[] = ['name' => $parts[0], 'health' => $parts[1]];
        }
        return $result;
    }

    public function getPoolStatus($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $statusMap = self::zpoolFetchAllStatus();
        $sizeMap   = self::zpoolFetchAllSizes();
        $result = [];
        foreach ($statusMap as $name => $info) {
            $result[] = [
                'name'     => $name,
                'state'    => $info['state'],
                'status'   => $info['status'],
                'capacity' => (int)($sizeMap[$name]['capacity'] ?? 0),
            ];
        }
        usort($result, fn($a, $b) => strcmp($a['name'], $b['name']));
        return $result;
    }

    public function getArcStats($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        if (!file_exists('/proc/spl/kstat/zfs/arcstats')) {
            return 'ARC statistics unavailable (/proc/spl/kstat/zfs/arcstats not found).';
        }
        if (is_executable('/usr/bin/zarcsummary')) {
            $cmd = '/usr/bin/zarcsummary 2>&1';
        } elseif (is_executable('/usr/sbin/arc_summary')) {
            $cmd = '/usr/sbin/arc_summary 2>&1';
        } else {
            return 'ARC summary tool not found (zarcsummary or arc_summary required).';
        }
        self::zfsExec($cmd, $out, $res);
        return implode("\n\r", $out);
    }

    public function getDatasetNames($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $result = [];
        if (!self::zfsTryExec('zfs list -H -t filesystem,volume -o name 2>&1', $out)) {
            return $result;
        }
        foreach ($out as $line) {
            $name = trim($line);
            if ($name !== '') {
                $result[] = [$name, $name];
            }
        }
        return $result;
    }

    public function getPoolNames($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $result = [];
        if (!self::zfsTryExec('zpool list -H -o name 2>&1', $out)) {
            return $result;
        }
        foreach ($out as $line) {
            $name = trim($line);
            if ($name !== '') {
                $result[] = [$name, $name];
            }
        }
        return $result;
    }

    // -------------------------------------------------------------------------
    // Snapshot scheduling
    // -------------------------------------------------------------------------

    public function getSnapshotJobList($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.getlist');
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->get('conf.service.zfs.snapshotjob');
        $result = array_map(fn($o) => $o->getAssoc(), $objects);
        return $this->applyFilter($result, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function getSnapshotJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.snapshotjob', $params['uuid']);
        $result = $obj->getAssoc();
        foreach (['minute', 'hour', 'dayofmonth', 'month', 'dayofweek'] as $f) {
            $result[$f] = array_map('trim', explode(',', $result[$f]));
        }
        return $result;
    }

    public function setSnapshotJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.setsnapshotjob');
        foreach (['minute', 'hour', 'dayofmonth', 'month', 'dayofweek'] as $f) {
            if (is_array($params[$f])) {
                $params[$f] = implode(',', $params[$f]);
            }
        }
        $db = \OMV\Config\Database::getInstance();
        $obj = new \OMV\Config\ConfigObject('conf.service.zfs.snapshotjob');
        $obj->setAssoc($params);
        $db->set($obj);
        return $obj->getAssoc();
    }

    public function deleteSnapshotJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.snapshotjob', $params['uuid']);
        $db->delete($obj);
    }

    public function runSnapshotJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.snapshotjob', $params['uuid']);
        $j = $obj->getAssoc();
        $cmd = sprintf('/usr/sbin/omv-zfs-snapshot %s %s %d %d 2>&1',
            escapeshellarg($j['dataset']),
            escapeshellarg($j['prefix']),
            (int)$j['retention'],
            !empty($j['recursive']) ? 1 : 0);
        exec($cmd, $out, $res);
        if ($res !== 0) {
            throw new \OMV\Exception('Snapshot job failed: ' . implode("\n", $out));
        }
        return implode("\n", $out);
    }

    public function runSnapshotJobBg($params, $context)
    {
        return $this->callMethodBg('runSnapshotJob', $params, $context);
    }

    // -------------------------------------------------------------------------
    // Scrub scheduling
    // -------------------------------------------------------------------------

    public function getScrubJobList($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.getlist');
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->get('conf.service.zfs.scrubjob');
        $result = array_map(fn($o) => $o->getAssoc(), $objects);
        return $this->applyFilter($result, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function getScrubJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.scrubjob', $params['uuid']);
        $result = $obj->getAssoc();
        foreach (['minute', 'hour', 'dayofmonth', 'month', 'dayofweek'] as $f) {
            $result[$f] = array_map('trim', explode(',', $result[$f]));
        }
        return $result;
    }

    public function setScrubJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.setscrubjob');
        foreach (['minute', 'hour', 'dayofmonth', 'month', 'dayofweek'] as $f) {
            if (is_array($params[$f])) {
                $params[$f] = implode(',', $params[$f]);
            }
        }
        $db = \OMV\Config\Database::getInstance();
        $obj = new \OMV\Config\ConfigObject('conf.service.zfs.scrubjob');
        $obj->setAssoc($params);
        $db->set($obj);
        return $obj->getAssoc();
    }

    public function deleteScrubJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.scrubjob', $params['uuid']);
        $db->delete($obj);
    }

    public function runScrubJob($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');
        $db = \OMV\Config\Database::getInstance();
        $obj = $db->get('conf.service.zfs.scrubjob', $params['uuid']);
        $j = $obj->getAssoc();
        self::zfsExec(sprintf('zpool scrub %s 2>&1', escapeshellarg($j['pool'])), $out, $res);
        return implode("\n", $out);
    }

    public function runScrubJobBg($params, $context)
    {
        return $this->callMethodBg('runScrubJob', $params, $context);
    }


    public function addPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.addpool');
        if (preg_match('/[\s]+/', $params['name'])) {
            throw new \OMV\Exception('Spaces are not allowed in pool name');
        }
        // Build zpool create options.
        $opts  = '';
        if ($params['force']) {
            $opts .= '-f ';
        }
        if ($params['ashift']) {
            $opts .= sprintf('-o ashift=%s ', $params['ashiftval']);
        }
        $opts .= '-o failmode=continue -o autoexpand=on -O atime=off -O acltype=posix -O xattr=sa ';
        if (strlen($params['mountpoint']) > 0) {
            $opts .= sprintf('-m %s ', escapeshellarg($params['mountpoint']));
        }
        // Resolve device aliases.
        $disks = $params['devices'];
        if ($params['devalias'] === 'path' && file_exists('/dev/disk/by-path/')) {
            try {
                $disks = array_map(['self', 'getDiskPath'], $disks);
            } catch (\Exception $e) {}
        } elseif ($params['devalias'] === 'id' && file_exists('/dev/disk/by-id/')) {
            try {
                $disks = array_map(['self', 'getDiskId'], $disks);
            } catch (\Exception $e) {}
        }
        // Create the pool.
        $cmd = sprintf('zpool create %s%s %s 2>&1',
            $opts,
            escapeshellarg($params['name']),
            self::buildVdevString($params['pooltype'], $disks));
        self::zfsExec($cmd, $out, $res);
        // Set compression if requested.
        if ($params['compress']) {
            $cmd = sprintf('zfs set compression=%s %s 2>&1',
                escapeshellarg($params['compresstype']),
                escapeshellarg($params['name']));
            self::zfsExec($cmd, $out, $res);
        }
        // Sync OMV mount entries and mark config modules dirty.
        self::fixOMVMntEnt($context);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function importPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.importpool');
        $cmd = 'zpool import ';
        if ($params['force']) {
            $cmd .= '-f ';
        }
        if ($params['all']) {
            $cmd .= '-a ';
        } elseif (strlen($params['poolname']) !== 0) {
            $cmd .= escapeshellarg($params['poolname']) . ' ';
        }
        $cmd .= '2>&1';
        self::zfsExec($cmd, $out, $res);
    }

    public function exportPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool export %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function clearPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool clear %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function trimPool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool trim %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function upgradePool($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool upgrade %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function addVdev($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.addvdev');
        $device = $params['device'];
        if ($params['devalias'] === 'id') {
            try { $device = self::getDiskId($device); } catch (\Throwable $e) {}
        } elseif ($params['devalias'] === 'path') {
            try { $device = self::getDiskPath($device); } catch (\Throwable $e) {}
        }
        // For log and special devices, prevent a dangerous stripe by
        // auto-mirroring: if one already exists, attach the new device instead.
        // A non-redundant special vdev risks pool corruption on device failure.
        $mirrorGuard = ['log' => 'findFirstLogDevice', 'special' => 'findFirstSpecialDevice'];
        if (isset($mirrorGuard[$params['vdevtype']])) {
            $finder   = $mirrorGuard[$params['vdevtype']];
            $existing = self::$finder($params['pool']);
            if ($existing !== null) {
                $cmd = sprintf('zpool attach %s %s %s 2>&1',
                    escapeshellarg($params['pool']),
                    escapeshellarg($existing),
                    escapeshellarg($device));
                self::zfsExec($cmd, $out, $res);
                return;
            }
        }
        // ZFS rejects a single-disk special vdev on a redundant pool (raidz,
        // mirror) without -f because the redundancy levels are mismatched.
        // The user has already acknowledged the risk via the UI warning, and
        // the mirror guard above will attach a second device as a mirror, so
        // -f is appropriate here.
        $force = ($params['vdevtype'] === 'special') ? '-f ' : '';
        $cmd = sprintf('zpool add %s%s %s %s 2>&1',
            $force,
            escapeshellarg($params['pool']),
            escapeshellarg($params['vdevtype']),
            escapeshellarg($device));
        self::zfsExec($cmd, $out, $res);
    }

    public function getSnapshotDiff($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        if (!self::zfsTryExec(sprintf('zfs diff %s 2>&1', escapeshellarg($params['name'])), $out)) {
            return sprintf("Unable to get diff for snapshot '%s'.", $params['name']);
        }
        if (empty($out)) {
            return 'No changes since this snapshot was taken.';
        }
        return implode("\n\r", $out);
    }

    public function listCompressionTypes($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = ['gzip'];
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('gzip-%d', $i);
        }
        $objects[] = 'lz4';
        $objects[] = 'lzjb';
        $objects[] = 'zle';
        $objects[] = 'zstd';
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('zstd-%d', $i);
        }
        $objects[] = 'zstd-fast';
        for ($i = 0; $i < 10; $i++) {
            $objects[] = sprintf('zstd-fast-%d', $i);
        }
        for ($i = 10; $i <= 100; $i += 10) {
            $objects[] = sprintf('zstd-fast-%d', $i);
        }
        $objects[] = 'zstd-fast-500';
        $objects[] = 'zstd-fast-1000';
        return $objects;
    }

    public function listPools($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = self::getZFSFlatArray();
        return $this->applyFilter($objects, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function listPoolsBg($params, $context)
    {
        return $this->callMethodBg('listPools', $params, $context);
    }

    public function listPoolsOnly($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = array_values(array_filter(self::getZFSFlatArray(), function($obj) {
            return $obj['type'] === 'Pool';
        }));
        return $this->applyFilter($objects, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function listPoolsOnlyBg($params, $context)
    {
        return $this->callMethodBg('listPoolsOnly', $params, $context);
    }

    public function listDatasets($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects = array_values(array_filter(self::getZFSFlatArray(), function($obj) {
            return in_array($obj['type'], ['Filesystem', 'Volume']);
        }));
        return $this->applyFilter($objects, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function listDatasetsBg($params, $context)
    {
        return $this->callMethodBg('listDatasets', $params, $context);
    }

    public function getAllSnapshots($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.common.getlist');
        $objects = self::getAllSnapshotsData();
        return $this->applyFilter($objects, $params['start'],
            $params['limit'], $params['sortfield'], $params['sortdir']);
    }

    public function getAllSnapshotsBg($params, $context)
    {
        return $this->callMethodBg('getAllSnapshots', $params, $context);
    }

    public function addObject($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.addobject');
        if (preg_match('/[\s]+/', $params['name'])) {
            throw new \OMV\Exception('Spaces are not allowed in object names');
        }
        switch ($params['type']) {
            case 'clone':
                $target = $params['pool'] . '/' . $params['clonename'];
                $cmd = sprintf('zfs clone -p %s %s 2>&1',
                    escapeshellarg($params['path']),
                    escapeshellarg($target));
                self::zfsExec($cmd, $out, $res);
                self::addOMVMntEntForDataset($context, $target);
                break;
            case 'filesystem':
                $name     = $params['path'] . '/' . $params['name'];
                $propOpts = '';
                if (!empty($params['compression'])) {
                    $propOpts .= ' -o compression=' . escapeshellarg($params['compression']);
                }
                if (!empty($params['recordsize'])) {
                    $propOpts .= ' -o recordsize=' . escapeshellarg($params['recordsize']);
                }
                $cmd = sprintf('zfs create -p%s %s 2>&1', $propOpts, escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                // Encrypted parent pools may not auto-mount new child datasets
                // when zfs create is called from a daemon process; mount
                // explicitly.  This is a no-op if the dataset is already mounted.
                self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($name)));
                if (strlen($params['mountpoint']) > 0) {
                    foreach (['mountpoint' => $params['mountpoint'], 'atime' => 'off',
                              'acltype' => 'posix', 'xattr' => 'sa'] as $k => $v) {
                        $cmd = sprintf('zfs set %s=%s %s 2>&1',
                            escapeshellarg($k), escapeshellarg($v), escapeshellarg($name));
                        self::zfsExec($cmd, $out, $res);
                    }
                }
                \OMV\Engine\Notify\Dispatcher::getInstance()->notify(OMV_NOTIFY_MODIFY,
                    'org.openmediavault.storage.zfs.filesystem', $context);
                self::addOMVMntEntForDataset($context, $name);
                // Re-mount after addOMVMntEntForDataset: setting omvzfsplugin:uuid on a
                // mounted encrypted dataset can clear ZFS's mounted-state tracking on
                // some kernel versions.  This call is a no-op if already mounted.
                self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($name)));
                break;
            case 'snapshot':
                $name      = $params['path'] . '@' . $params['name'];
                $recursive = !empty($params['recursive']) ? '-r ' : '';
                $cmd = sprintf('zfs snapshot %s%s 2>&1', $recursive, escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                break;
            case 'volume':
                $name    = $params['path'] . '/' . $params['name'];
                $volsize = str_replace([' ', 'i'], '', $params['size']);
                $sparse  = boolval($params['thinvol']) ? '-s ' : '';
                $cmd = sprintf('zfs create -p %s-V %s %s 2>&1',
                    $sparse, escapeshellarg($volsize), escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                break;
            default:
                throw new \OMV\Exception('Illegal type provided: ' . $params['type']);
        }
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function quickSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.quicksnapshot');
        $snapshotName = $params['name'] . '@' . date('Y-m-d_H-i-s');
        $recursive    = !empty($params['recursive']) ? '-r ' : '';
        $cmd = sprintf('zfs snapshot %s%s 2>&1', $recursive, escapeshellarg($snapshotName));
        self::zfsExec($cmd, $out, $res);
    }

    public function renameObject($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.rename');
        $oldPath = $params['oldpath'];
        $parts   = explode('/', $oldPath);
        array_pop($parts);
        $newPath = implode('/', $parts) . '/' . $params['newname'];
        $cmd = sprintf('zfs rename %s %s 2>&1', escapeshellarg($oldPath), escapeshellarg($newPath));
        try {
            self::zfsExec($cmd, $out, $res);
        } catch (\Exception $e) {
            if (stripos($e->getMessage(), 'busy') !== false) {
                throw new \OMV\Exception(
                    self::buildBusyRenameError($context, $oldPath)
                );
            }
            throw $e;
        }
        self::fixOMVMntEnt($context, true, true);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function cloneDataset($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.clonedataset');
        $source    = $params['name'];
        $clonename = $params['clonename'];
        if (preg_match('/[\s]+/', $clonename)) {
            throw new \OMV\Exception('Spaces are not allowed in clone names');
        }
        // Build target path: same parent as source, with clonename as leaf.
        $parts  = explode('/', $source);
        array_pop($parts);
        $target = implode('/', $parts) . '/' . $clonename;
        // Auto-create a snapshot to clone from.
        $snapshot = $source . '@clone-' . date('Y-m-d_H-i-s');
        self::zfsExec(sprintf('zfs snapshot %s 2>&1', escapeshellarg($snapshot)), $out, $res);
        // Clone from the snapshot.
        $cmd = sprintf('zfs clone -p %s %s 2>&1', escapeshellarg($snapshot), escapeshellarg($target));
        self::zfsExec($cmd, $out, $res);
        self::addOMVMntEntForDataset($context, $target);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function promoteDataset($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zfs promote %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
        self::fixOMVMntEnt($context);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function deleteObject($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.deleteobject');
        $name = $params['name'];
        $mp   = $params['mp'] ?? '';
        switch ($params['type']) {
            case 'Filesystem':
                if (self::hasChildren($name)) {
                    throw new \OMV\Exception(
                        'Cannot delete filesystem "%s": it has child datasets. Delete them first.', $name);
                }
                if (self::checkOMVShares($context, $mp)) {
                    throw new \OMV\Exception(
                        'Before deleting this filesystem, you must delete shares referencing it.');
                }
                self::deleteOMVMntEnt($context, $name, $mp);
                self::cleanupJobsForDataset($name);
                \OMV\Engine\Notify\Dispatcher::getInstance()->notify(OMV_NOTIFY_MODIFY,
                    'org.openmediavault.storage.zfs.filesystem', $context);
                break;
            case 'Snapshot':
                $recursive = !empty($params['recursive']) ? '-r ' : '';
                $cmd = sprintf('zfs destroy %s%s 2>&1', $recursive, escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                break;
            case 'Volume':
                if (self::hasChildren($name)) {
                    throw new \OMV\Exception(
                        'Cannot delete volume "%s": it has child datasets. Delete them first.', $name);
                }
                $cmd = sprintf('zfs destroy %s 2>&1', escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                self::cleanupJobsForDataset($name);
                break;
            case 'Pool':
                if (self::hasChildren($name)) {
                    throw new \OMV\Exception(
                        'Cannot delete pool "%s": it has datasets. Delete them first.', $name);
                }
                if (self::checkOMVShares($context, $mp)) {
                    throw new \OMV\Exception(
                        'Before deleting this pool, you must delete shares referencing it.');
                }
                // Remove the fstab entry for the pool root.
                $fstabObj = \OMV\Rpc\Rpc::call('FsTab', 'getByDir', ['dir' => $mp], $context);
                if ($fstabObj && $fstabObj['type'] === 'zfs') {
                    \OMV\Rpc\Rpc::call('FsTab', 'delete', ['uuid' => $fstabObj['uuid']], $context);
                }
                // Collect devices before destroying so we can clear labels.
                $devices = self::zpoolGetAllDevices($name, ['INUSE', 'UNAVAIL', 'SPLIT']);
                $cmd = sprintf('zpool destroy %s 2>&1', escapeshellarg($name));
                self::zfsExec($cmd, $out, $res);
                foreach ($devices as $device) {
                    self::zfsTryExec(sprintf('zpool labelclear %s 2>&1', escapeshellarg($device)));
                    self::zfsTryExec(sprintf('wipefs -a %s 2>&1', escapeshellarg($device)));
                    print_r("wipefs");
                }
                self::cleanupJobsForPool($name);
                break;
            default:
                throw new \OMV\Exception('Illegal type provided: ' . $params['type']);
        }
        // Sync OMV mount entries and mark config modules dirty.
        self::fixOMVMntEnt($context);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
        // Keep zfs-list.cache consistent so zfs-mount-generator doesn't
        // generate stale systemd units for destroyed datasets/pools.
        if ($params['type'] === 'Pool') {
            @unlink('/etc/zfs/zfs-list.cache/' . $name);
        } elseif ($params['type'] === 'Filesystem') {
            self::updateZfsListCache(explode('/', $name)[0]);
            // Clean up encryption artifacts so stale keyfiles and remote-key metadata
            // do not accumulate for deleted datasets and cause manageRemoteUnlockService()
            // to incorrectly report that remote unlock is still active.
            $keyfile = self::getKeyfilePath($name);
            if (file_exists($keyfile)) {
                @unlink($keyfile);
            }
            $metafile = self::getRemoteKeyMetaPath($name);
            if (file_exists($metafile)) {
                @unlink($metafile);
                self::manageRemoteUnlockService();
            }
            // Unmask and stop the zfs-load-key@ unit for the deleted dataset.
            // Without unmasking, a stale /dev/null symlink from removeAutoUnlock
            // remains in /etc/systemd/system/ as an orphaned artifact.
            $instanceOut = [];
            exec(sprintf('systemd-escape -p %s 2>/dev/null', escapeshellarg($name)), $instanceOut);
            $instance = trim(implode('', $instanceOut));
            if ($instance !== '') {
                $loadKeyUnit = 'zfs-load-key@' . $instance . '.service';
                self::zfsTryExec(sprintf('systemctl unmask %s 2>/dev/null', escapeshellarg($loadKeyUnit)));
                self::zfsTryExec(sprintf('systemctl stop %s 2>/dev/null', escapeshellarg($loadKeyUnit)));
            }
        }
        // Reload systemd so zfs-mount-generator re-runs and drops generated
        // unit files for the deleted dataset/pool from /run/systemd/generator/.
        self::zfsTryExec('systemctl daemon-reload 2>/dev/null');
    }

    public function deleteObjectBg($params, $context)
    {
        return $this->callMethodBg('deleteObject', $params, $context);
    }

    public function deleteSnapshotRange($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.deletesnapshotrange');
        $name = $params['name'];
        $mode = $params['mode'];
        // Extract parent dataset and snapshot name from path (e.g. pool/data@snap1).
        if (!preg_match('/^(.+)@(.+)$/', $name, $m)) {
            throw new \OMV\Exception('Invalid snapshot path: %s', $name);
        }
        $parent = $m[1];
        $snap   = $m[2];
        switch ($mode) {
            case 'earlier':
                // Destroy the named snapshot and all earlier snapshots.
                $destroyArg = $parent . '@%' . $snap;
                break;
            case 'later':
                // Destroy the named snapshot and all later snapshots.
                $destroyArg = $parent . '@' . $snap . '%';
                break;
            case 'all':
                // Destroy all snapshots on the parent dataset.
                $destroyArg = $parent . '@%';
                break;
            default:
                throw new \OMV\Exception('Invalid mode: %s', $mode);
        }
        $cmd = sprintf('zfs destroy %s 2>&1', escapeshellarg($destroyArg));
        self::zfsExec($cmd, $out, $res);
        self::fixOMVMntEnt($context);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
    }

    public function deleteSnapshotRangeBg($params, $context)
    {
        return $this->callMethodBg('deleteSnapshotRange', $params, $context);
    }

    public function getProperties($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $objects    = [];
        $properties = ($params['type'] === 'Pool')
            ? self::zpoolGetAllPropertiesRaw($params['name'])
            : self::zfsGetAllPropertiesRaw($params['name']);
        foreach ($properties as $propertyk => $propertyv) {
            if ($propertyv['source'] === '-') {
                continue;
            }
            $objects[] = [
                'property' => $propertyk,
                'value'    => $propertyv['value'],
                'source'   => $propertyv['source'],
                'modified' => 'false',
            ];
        }
        return ['properties' => $objects];
    }

    public function setProperties($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.setproperties');
        $isPool = ($params['type'] === 'Pool');
        foreach ($params['properties'] as $property) {
            if ($property['modified'] !== true) {
                continue;
            }
            if ($isPool) {
                $cmd = sprintf('zpool set %s=%s %s 2>&1',
                    escapeshellarg($property['property']),
                    escapeshellarg($property['value']),
                    escapeshellarg($params['name']));
                self::zfsExec($cmd, $out, $res);
            } else {
                $cmd = sprintf('zfs set %s=%s %s 2>&1',
                    escapeshellarg($property['property']),
                    escapeshellarg($property['value']),
                    escapeshellarg($params['name']));
                self::zfsExec($cmd, $out, $res);
                if ($property['property'] === 'mountpoint') {
                    self::relocateOMVMntEnt($context, $params['name']);
                    \OMV\Engine\Notify\Dispatcher::getInstance()->notify(OMV_NOTIFY_MODIFY,
                        'org.openmediavault.conf.system.sharedfolder', $context);
                }
            }
        }
        if ($params['type'] === 'Filesystem') {
            \OMV\Engine\Notify\Dispatcher::getInstance()->notify(OMV_NOTIFY_MODIFY,
                'org.openmediavault.storage.zfs.filesystem', $context);
            self::dirtyModules(['collectd', 'monit', 'quota']);
        }
    }

    public function getObjectDetails($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.objectdetails');

        $name = escapeshellarg($params['name']);
        $output = '';

        switch ($params['type']) {
            case 'Filesystem':
                self::zfsExec("zfs list -o name,type,used,avail,refer,mountpoint,compression,compressratio,encryption $name", $out, $res);
                $output  = "Filesystem status (zfs list):\n\r\n\r" . implode("\n\r", $out);
                $output .= "\n\r\n\rFilesystem details (zfs get all):\n\r\n\r";
                break;

            case 'Volume':
                $output = "Volume details (zfs get all):\n\r\n\r";
                break;

            case 'Snapshot':
                $output = "Snapshot details (zfs get all):\n\r\n\r";
                break;

            case 'Pool':
                self::zfsExec("zpool status -v $name", $out, $res);
                return "Pool status (zpool status -v):\n\r\n\r" . implode("\n\r", $out);

            default:
                throw new \OMV\Exception('Incorrect type provided');
        }

        self::zfsExec("zfs get all $name", $out, $res);
        $output .= implode("\n\r", $out);

        return $output;
    }

    public function expandPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.expandpool');
        if (empty($params['name'])) {
            throw new \OMV\Exception('Pool name cannot be empty');
        }
        if (!is_array($params['devices']) || empty($params['devices'])) {
            throw new \OMV\Exception('No devices selected for expansion');
        }
        // Validate minimum disk requirements.
        $minDisks = ['mirror' => 2, 'raidz1' => 3, 'raidz2' => 4, 'raidz3' => 5];
        $diskCount = count($params['devices']);
        if (isset($minDisks[$params['vdevtype']]) && $diskCount < $minDisks[$params['vdevtype']]) {
            throw new \OMV\Exception(sprintf('Not enough disks for %s', $params['vdevtype']));
        }
        // Resolve device aliases.
        $disks = $params['devices'];
        if ($params['devalias'] === 'path' && file_exists('/dev/disk/by-path/')) {
            try {
                $disks = array_map(['self', 'getDiskPath'], $disks);
            } catch (\Exception $e) {}
        } elseif ($params['devalias'] === 'id' && file_exists('/dev/disk/by-id/')) {
            try {
                $disks = array_map(['self', 'getDiskId'], $disks);
            } catch (\Exception $e) {}
        }
        $opts = $params['force'] ? '-f ' : '';
        $cmd = sprintf('zpool add %s%s %s 2>&1',
            $opts,
            escapeshellarg($params['name']),
            self::buildVdevString($params['vdevtype'], $disks));
        self::zfsExec($cmd, $out, $res);
    }

    public function getPoolDevices($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $pool = $params['name'];
        $cmd  = sprintf('zpool status -j %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $json    = json_decode(implode('', $out), true);
        $poolObj = $json['pools'][$pool] ?? null;
        if ($poolObj === null) {
            return [];
        }
        $devices  = [];
        $sections = array_merge(
            array_values($poolObj['vdevs'][$pool]['vdevs'] ?? []),
            array_values($poolObj['logs']    ?? []),
            array_values($poolObj['l2cache'] ?? []),
            array_values($poolObj['special'] ?? []),
            array_values($poolObj['spares']  ?? [])
        );
        foreach ($sections as $vdev) {
            self::collectDevicesRecursive($vdev, '', $devices);
        }
        return $devices;
    }

    /**
     * Return the list of valid attach anchors for a pool:
     *  - For mirror / stripe pools: the individual device paths (same as getPoolDevices).
     *  - For RAIDZ vdevs where feature@raidz_expansion is active: the vdev name
     *    (e.g. "raidz1-0") is returned instead of the member devices, since
     *    "zpool attach <pool> <raidz-vdev> <new-device>" is how RAIDZ expansion works.
     *    RAIDZ member devices are omitted from the list when the feature is active so
     *    the selector only shows meaningful choices.
     */
    public function getAttachAnchors($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $pool = $params['name'];

        // Check whether raidz_expansion is enabled or active for this pool.
        // The feature state is 'enabled' once set, and transitions to 'active'
        // after it has been used.  Both states permit RAIDZ expansion.
        $expansionActive = false;
        $featCmd = sprintf('zpool get -H -o value feature@raidz_expansion %s 2>/dev/null',
            escapeshellarg($pool));
        if (self::zfsTryExec($featCmd, $featOut)) {
            $expansionActive = in_array(trim($featOut[0] ?? ''), ['enabled', 'active']);
        }

        $cmd = sprintf('zpool status -j %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $json    = json_decode(implode('', $out), true);
        $poolObj = $json['pools'][$pool] ?? null;
        if ($poolObj === null) {
            return [];
        }
        $anchors = [];
        // Only data vdevs are valid attach anchors (not log/cache/spare/special).
        // zpool attach is only meaningful for mirrors and stripe devices.
        foreach ($poolObj['vdevs'][$pool]['vdevs'] ?? [] as $vdev) {
            $name = $vdev['name'] ?? '';
            $type = self::vdevTypeFromJson($vdev['vdev_type'] ?? '', $name);
            if (str_starts_with($type, 'raidz')) {
                // For RAIDZ vdevs with expansion enabled, add the vdev name as anchor.
                if ($expansionActive) {
                    $anchors[] = [
                        'devicefile' => $name,
                        'state'      => '',
                        'vdev'       => $name,
                        'label'      => $name . '  [RAIDZ expansion]',
                    ];
                }
                // Never add RAIDZ member devices — zpool attach is invalid on them.
                continue;
            }
            self::collectDevicesRecursive($vdev, '', $anchors);
        }
        return $anchors;
    }

    public function getTopLevelVdevs($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $pool = $params['name'];
        $cmd  = sprintf('zpool status -j %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $json    = json_decode(implode('', $out), true);
        $poolObj = $json['pools'][$pool] ?? null;
        if ($poolObj === null) {
            return [];
        }
        $vdevs = [];
        // Data vdevs — raidz vdevs are not removable via zpool remove.
        foreach ($poolObj['vdevs'][$pool]['vdevs'] ?? [] as $vdev) {
            $name  = $vdev['name'] ?? '';
            $state = strtoupper($vdev['state'] ?? 'UNKNOWN');
            $type  = self::vdevTypeFromJson($vdev['vdev_type'] ?? '', $name);
            if (str_starts_with($type, 'raidz')) {
                continue;
            }
            $vdevs[] = [
                'vdev'  => $name,
                'type'  => $type,
                'state' => $state,
                'label' => 'Data: ' . $name . '  [' . $type . '] [' . $state . ']',
            ];
        }
        // Class vdevs (logs, l2cache, special, spares).
        $classMap = ['logs' => 'log', 'l2cache' => 'cache', 'special' => 'special',
                     'spares' => 'spare', 'dedup' => 'dedup'];
        foreach ($classMap as $section => $classType) {
            foreach ($poolObj[$section] ?? [] as $vdev) {
                $name  = $vdev['name'] ?? '';
                $state = strtoupper($vdev['state'] ?? 'UNKNOWN');
                $type  = self::vdevTypeFromJson($vdev['vdev_type'] ?? '', $name);
                $vdevs[] = [
                    'vdev'  => $name,
                    'type'  => $classType,
                    'state' => $state,
                    'label' => ucfirst($classType) . ': ' . $name . '  [' . $type . '] [' . $state . ']',
                ];
            }
        }
        return $vdevs;
    }

    public function removeVdev($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.poolvdev');
        $cmd = sprintf('zpool remove %s %s 2>&1',
            escapeshellarg($params['pool']),
            escapeshellarg($params['vdev']));
        self::zfsExec($cmd, $out, $res);
    }

    public function cancelVdevRemoval($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool remove -s %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function getVdevRemovalStatus($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        self::zfsExec(sprintf('zpool status %s 2>&1', escapeshellarg($params['name'])), $out, $res);
        $lines    = [];
        $inRemove = false;
        foreach ($out as $line) {
            if (preg_match('/^\s*remove\s*:/', $line)) {
                $inRemove = true;
                $lines[]  = trim($line);
            } elseif ($inRemove) {
                if (preg_match('/^\s*\w+\s*:/', $line)) {
                    break;
                }
                $lines[] = trim($line);
            }
        }
        if (empty($lines)) {
            return sprintf("No vdev removal in progress for pool '%s'.", $params['name']);
        }
        return implode("\n\r", $lines);
    }

    public function deviceOffline($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.pooldevice');
        $cmd = sprintf('zpool offline %s %s 2>&1',
            escapeshellarg($params['pool']), escapeshellarg($params['device']));
        self::zfsExec($cmd, $out, $res);
    }

    public function deviceOnline($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.pooldevice');
        // zpool online requires the whole-disk path to read the label/capacity.
        // Strip the partition suffix so ZFS can relabel the disk correctly:
        //   /dev/disk/by-id/foo-part1  -> /dev/disk/by-id/foo  (by-id / by-path)
        //   /dev/sda1                  -> /dev/sda              (raw device)
        $device = preg_replace('/-part\d+$/', '', $params['device']);
        if ($device === $params['device']) {
            // No -partN suffix was found; try stripping a trailing digit from a
            // short /dev/sdXN-style path, being careful not to touch by-id paths
            // whose disk IDs end in digits.
            $device = preg_replace('#^(/dev/[a-z]+)\d+$#', '$1', $device);
        }
        $cmd = sprintf('zpool online %s %s 2>&1',
            escapeshellarg($params['pool']), escapeshellarg($device));
        self::zfsExec($cmd, $out, $res);
    }

    public function deviceDetach($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.pooldevice');
        $cmd = sprintf('zpool detach %s %s 2>&1',
            escapeshellarg($params['pool']), escapeshellarg($params['device']));
        if (!self::zfsTryExec($cmd, $out)) {
            $msg = !empty($out) ? implode(' ', array_map('trim', $out)) : 'zpool detach failed.';
            throw new \OMV\Exception($msg);
        }
    }

    public function deviceReplace($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.pooltwodevices');
        $cmd = sprintf('zpool replace %s %s %s 2>&1',
            escapeshellarg($params['pool']),
            escapeshellarg($params['olddevice']),
            escapeshellarg($params['newdevice']));
        self::zfsExec($cmd, $out, $res);
    }

    public function deviceAttach($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.pooltwodevices');
        $cmd = sprintf('zpool attach %s %s %s 2>&1',
            escapeshellarg($params['pool']),
            escapeshellarg($params['olddevice']),
            escapeshellarg($params['newdevice']));
        self::zfsExec($cmd, $out, $res);
    }

    public function scrubPool($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool scrub %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function stopScrub($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool scrub -s %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function pauseScrub($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zpool scrub -p %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function getScrubStatus($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        self::zfsExec(sprintf('zpool status %s 2>&1', escapeshellarg($params['name'])), $out, $res);
        $scrubLines = [];
        $inScan = false;
        foreach ($out as $line) {
            if (preg_match('/^\s*scan:/', $line)) {
                $inScan = true;
                $scrubLines[] = trim($line);
            } elseif ($inScan) {
                // Stop at the next field label (e.g. "config:", "errors:")
                if (preg_match('/^\s*\w+:/', $line)) {
                    break;
                }
                $scrubLines[] = trim($line);
            }
        }
        if (empty($scrubLines)) {
            return sprintf("No scrub information available for pool '%s'.", $params['name']);
        }
        return implode("\n\r", $scrubLines);
    }

    public function getPoolHistory($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        if (!self::zfsTryExec(sprintf('zpool history %s 2>&1', escapeshellarg($params['name'])), $out)) {
            return sprintf("Failed to retrieve history for pool '%s'.", $params['name']);
        }
        if (empty($out)) {
            return sprintf("No history available for pool '%s'.", $params['name']);
        }
        return implode("\n\r", $out);
    }

    public function getPoolEvents($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        if (!self::zfsTryExec('zpool events 2>&1', $out)) {
            return 'ZFS event log unavailable.';
        }
        if (empty($out)) {
            return 'No ZFS events logged.';
        }
        return implode("\n\r", $out);
    }

    public function getEmptyCandidates($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        if (false === ($devs = \OMV\System\Storage\StorageDevice::enumerateUnused())) {
            throw new \OMV\Exception('Failed to get list of unused devices.');
        }
        $result = [];
        $db = \OMV\Config\Database::getInstance();
        foreach ($devs as $devv) {
            $sd = \OMV\System\Storage\StorageDevice::getStorageDevice($devv);
            if (is_null($sd) || !$sd->exists()) {
                continue;
            }
            if ($sd->isReadOnly() || $sd->isUsb()) {
                continue;
            }
            if ($db->exists('conf.service', [
                'operator' => 'stringContains',
                'arg0'     => 'devicefile',
                'arg1'     => $sd->getDeviceFile(),
            ])) {
                continue;
            }
            if (false !== \OMV\System\Filesystem\Filesystem::hasFileSystem($sd->getDeviceFile())) {
                continue;
            }
            // Prefer stable by-id path over kernel device name to avoid
            // mixed naming in pools when attaching or replacing devices.
            $devfile = $sd->getDeviceFile();
            if (file_exists('/dev/disk/by-id/')) {
                try { $devfile = self::getDiskId($devfile); } catch (\Exception $e) {}
            }
            $result[] = [
                'devicefile'   => $devfile,
                'size'         => $sd->getSize(),
                'vendor'       => $sd->getVendor(),
                'serialnumber' => $sd->getSerialNumber(),
                'description'  => $sd->getDescription(),
            ];
        }
        return $result;
    }

    public function rollbackSnapshot($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $cmd = sprintf('zfs rollback %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
    }

    public function fixPackages($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        \OMV\Rpc\Rpc::call('Config', 'applyChanges',
            ['modules' => ['zfs'], 'force' => true], $context);
    }

    public function enableEncryption($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.enableencryption');
        if (preg_match('/[\s]+/', $params['name'])) {
            throw new \OMV\Exception('Spaces are not allowed in dataset names');
        }
        $fullPath    = rtrim($params['path'], '/') . '/' . $params['name'];
        $encryptOpts = sprintf('-o encryption=%s -o keyformat=passphrase -o keylocation=prompt',
            escapeshellarg($params['encryptiontype']));
        if (!empty($params['compression'])) {
            $encryptOpts .= ' -o compression=' . escapeshellarg($params['compression']);
        }
        if (!empty($params['recordsize'])) {
            $encryptOpts .= ' -o recordsize=' . escapeshellarg($params['recordsize']);
        }
        $cmd = sprintf('zfs create %s %s 2>&1', $encryptOpts, escapeshellarg($fullPath));
        self::execWithStdin($cmd, $params['key']);
        // zfs create for an encrypted dataset may not auto-mount when run from
        // a daemon process; mount explicitly so the dataset is immediately usable.
        self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($fullPath)));
        // Enable auto-unlock if requested: save keyfile and update keylocation.
        if (!empty($params['autounlock'])) {
            $keyfile = self::getKeyfilePath($fullPath);
            $keydir  = dirname($keyfile);
            if (!is_dir($keydir)) {
                mkdir($keydir, 0700, true);
            }
            file_put_contents($keyfile, $params['key']);
            chmod($keyfile, 0600);
            self::zfsExec(sprintf('zfs set keylocation=%s %s 2>&1',
                escapeshellarg('file://' . $keyfile),
                escapeshellarg($fullPath)), $out, $res);
            self::updateZfsListCache(explode('/', $fullPath)[0]);
        }
        self::addOMVMntEntForDataset($context, $fullPath);
        // Re-mount after addOMVMntEntForDataset: setting omvzfsplugin:uuid on a
        // mounted encrypted dataset can clear ZFS's mounted-state tracking on
        // some kernel versions.  This call is a no-op if already mounted.
        self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($fullPath)));
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
        $status = self::zfsGetEncryptionStatus($fullPath);
        return self::flattenEncryptionStatus($fullPath, $status);
    }

    public function loadEncryptionKey($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.loadencryptionkey');
        // A clone that has not been promoted shares its encryptionroot with the
        // origin dataset.  ZFS requires you to load the key on the root, not on
        // the clone, so give a clear error instead of a cryptic ZFS message.
        $encStatus = self::zfsGetEncryptionStatus($params['name']);
        $encRoot   = $encStatus['encryptionroot']['value'] ?? $params['name'];
        if ($encRoot !== '-' && $encRoot !== $params['name']) {
            throw new \OMV\Exception(
                "Cannot unlock '%s' independently — its encryption root is '%s'. "
                . "Unlock '%s' instead; this dataset will become accessible automatically.",
                $params['name'], $encRoot, $encRoot
            );
        }
        $cmd = 'zfs load-key ' . escapeshellarg($params['name']);
        self::execWithStdin($cmd, $params['key']);
        // Find every filesystem sharing this encryption root.  Filesystems only —
        // zvols have no mountpoint so zfs mount on them is invalid.
        // We scan individually rather than using 'zfs mount -R' because -R
        // recurses into sub-encryption-roots whose keys are unavailable, making
        // the command exit non-zero and leaving ALL datasets unmounted.
        // Individual mounts cover both hierarchical children (which inherit
        // this key) and sibling clones (which share the encryptionroot but
        // live at a different path in the pool hierarchy).
        $poolName = explode('/', $params['name'])[0];
        $toMount  = [];
        if (self::zfsTryExec(
            sprintf('zfs get -H -r -t filesystem -o name,property,value encryptionroot,mountpoint,canmount %s 2>/dev/null',
                escapeshellarg($poolName)),
            $propLines
        )) {
            $props = [];
            foreach ($propLines as $line) {
                $parts = explode("\t", trim($line));
                if (count($parts) === 3) {
                    $props[$parts[0]][$parts[1]] = $parts[2];
                }
            }
            foreach ($props as $ds => $p) {
                if (($p['encryptionroot'] ?? '') !== $params['name']) {
                    continue;
                }
                // Skip datasets that cannot be mounted: zfs mount will never
                // bring these up, so requiring mounted=yes would falsely fail.
                $mp = $p['mountpoint'] ?? '';
                if (in_array($mp, ['none', 'legacy', '-']) || ($p['canmount'] ?? 'on') === 'off') {
                    continue;
                }
                $toMount[] = $ds;
            }
        }
        // Fall back to the root dataset if the pool scan found nothing.
        if (empty($toMount)) {
            $toMount[] = $params['name'];
        }
        // Lexical sort is sufficient for parent-before-child ordering: a parent
        // path is always a prefix of its children, so it sorts first.
        sort($toMount);
        // Before mounting, ensure the systemd zfs-load-key@ service unit for
        // each dataset is in the 'active' state.  The systemd zfs-mount-generator
        // creates a .mount unit for every ZFS dataset with BindsTo pointing at
        // the corresponding zfs-load-key@ service.  If that service is in the
        // 'failed' state (as it will be after a reboot where no passphrase was
        // entered), systemd detects the dataset appearing in /proc/mounts,
        // activates the mount unit, then immediately deactivates it (~2ms) because
        // its BindsTo dependency is failed — effectively unmounting the dataset
        // right after zfs mount succeeds.
        //
        // The load-key service ExecStart checks keystatus and exits 0 immediately
        // if the key is already loaded (not "unavailable"), and RemainAfterExit=yes
        // keeps the service 'active' once it exits 0.  So:
        //   1. systemctl reset-failed <unit>  — clears the failed state
        //   2. systemctl start <unit>         — exits 0 immediately (key is loaded),
        //                                       transitions service to 'active'
        // With the service active, the BindsTo is satisfied and the mount unit
        // stays up permanently.
        $maskedLoadKeyUnits = [];
        foreach ($toMount as $ds) {
            $instanceOut = [];
            exec(sprintf('systemd-escape -p %s 2>/dev/null', escapeshellarg($ds)), $instanceOut);
            $instance = trim(implode('', $instanceOut));
            if ($instance !== '') {
                $loadKeyUnit = sprintf('zfs-load-key@%s.service', $instance);
                // A masked load-key unit cannot start, which causes the associated
                // .mount unit to fail its dependency check and fall into a failed
                // state.  Child datasets whose .mount unit has Requires=parent.mount
                // are then immediately unmounted by systemd right after zfs mount
                // succeeds.  Temporarily unmask so the service can start (it exits 0
                // immediately since the key is now loaded); we re-mask after all
                // mounts are verified so the removeAutoUnlock setting is preserved.
                $maskFile = '/etc/systemd/system/' . $loadKeyUnit;
                if (is_link($maskFile) && readlink($maskFile) === '/dev/null') {
                    self::zfsTryExec(sprintf('systemctl unmask %s 2>/dev/null',
                        escapeshellarg($loadKeyUnit)));
                    $maskedLoadKeyUnits[] = $loadKeyUnit;
                }
                self::zfsTryExec(sprintf('systemctl reset-failed %s 2>/dev/null', escapeshellarg($loadKeyUnit)));
                self::zfsTryExec(sprintf('systemctl start %s 2>/dev/null', escapeshellarg($loadKeyUnit)));
            }
        }
        // Mount first, register OMV entries after.  Calling
        // addOMVMntEntForDataset (which writes the omvzfsplugin:uuid user
        // property via zfs set) on an encrypted dataset before it is mounted
        // triggers a ZFS kernel-module bug on OpenZFS 2.4.x-pve1: the property
        // write puts the module's internal mount-state tracking into an
        // inconsistent "already mounted" ghost state, causing the subsequent
        // zfs mount to silently succeed while the dataset never appears in
        // /proc/mounts.  Mounting first avoids the ghost state; the user
        // property is written after the mounts are verified.  The property
        // write may then clear ZFS's mounted tracking (zfs get mounted → "no"),
        // but the dataset remains accessible and the UI shows the correct
        // mounted status by checking /proc/mounts directly as a fallback.
        //
        // Verify every dataset is actually mounted by checking /proc/mounts
        // directly.  zfs get mounted reads ZFS's internal tracking, which can
        // be out of sync with the real kernel mount state (e.g. after a failed
        // boot-time mount attempt left the tracking half-initialised).
        // Read /proc/mounts fresh on each call so post-recovery state is correct.
        $isProcMounted = static function (string $dataset): bool {
            $lines  = @file('/proc/mounts', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) ?: [];
            $prefix = $dataset . ' ';
            foreach ($lines as $line) {
                if (strncmp($line, $prefix, strlen($prefix)) === 0) {
                    return true;
                }
            }
            return false;
        };
        $mountErrors = [];
        foreach ($toMount as $ds) {
            $mountOut = [];
            self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($ds)), $mountOut);
            if ($isProcMounted($ds)) {
                continue;
            }
            // Not in /proc/mounts.  If ZFS reports "already mounted" its
            // internal tracking is stale — typically left by a failed
            // boot-time mount attempt that partially initialised the tracking
            // but never completed the OS-level mount.  Clear both the ZFS
            // tracking and any ghost kernel state at the mountpoint, then retry.
            if (stripos(implode(' ', $mountOut), 'already mounted') !== false) {
                $mp = trim($props[$ds]['mountpoint'] ?? '');
                // Force-unmount via ZFS first, then clean up any lingering
                // kernel mount at the same path.
                self::zfsTryExec(sprintf('zfs unmount -f %s 2>&1', escapeshellarg($ds)));
                if ($mp !== '') {
                    self::zfsTryExec(sprintf('umount -lf %s 2>/dev/null', escapeshellarg($mp)));
                }
                $mountOut = [];
                self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($ds)), $mountOut);
                if ($isProcMounted($ds)) {
                    continue;
                }
            }
            $detail = trim(implode(' ', $mountOut));
            $pool   = explode('/', $ds)[0];
            $mountErrors[] = $detail !== ''
                ? sprintf('%s: %s', $ds, $detail)
                : sprintf(
                    '%s: zfs mount reported success but the dataset is not mounted. '
                    . 'The ZFS kernel module has stale mount state from the last reboot '
                    . '(known OpenZFS 2.4.x-pve1 bug). '
                    . 'To recover: run "zpool export -f %s && zpool import %s" '
                    . 'in a shell, then load the key again.',
                    $ds, $pool, $pool
                );
        }
        if (!empty($mountErrors)) {
            throw new \OMV\Exception(
                'Key loaded but failed to mount: %s',
                implode('; ', $mountErrors)
            );
        }
        // Re-mask load-key units that were temporarily unmasked above.  The service
        // remains active (RemainAfterExit=yes) so the .mount BindsTo dependency stays
        // satisfied; the mask only prevents the unit from auto-starting at next boot.
        foreach ($maskedLoadKeyUnits as $unit) {
            self::zfsTryExec(sprintf('systemctl mask %s 2>/dev/null', escapeshellarg($unit)));
        }
        // All datasets are mounted and verified via /proc/mounts.  Register OMV
        // mntent entries but skip the omvzfsplugin:uuid property write.  On
        // OpenZFS 2.4.x-pve1, writing any user property on a mounted encrypted
        // dataset physically unmounts it, leaving it inaccessible.  The property
        // is written by doDiscoverBg when the UUID is absent or mismatched; that
        // path is safe because it runs asynchronously after this call returns.
        foreach ($toMount as $ds) {
            self::addOMVMntEntForDataset($context, $ds, false);
        }
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
        return ['status' => 'loaded'];
    }

    public function unloadEncryptionKey($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.unloadencryptionkey');
        // Refuse if this dataset is not its own encryption root.  Unloading the key
        // on a non-promoted clone would actually lock the shared encryptionroot and
        // all other datasets that inherit from it — very surprising behaviour.
        $encStatus = self::zfsGetEncryptionStatus($params['name']);
        $encRoot   = $encStatus['encryptionroot']['value'] ?? $params['name'];
        if ($encRoot !== '-' && $encRoot !== $params['name']) {
            throw new \OMV\Exception(
                "Cannot lock '%s' independently — its encryption root is '%s'. "
                . "Lock '%s' instead, which will also lock this dataset.",
                $params['name'], $encRoot, $encRoot
            );
        }
        // Unmount every dataset that shares this encryption root before unloading
        // the key.  Clones inherit the encryptionroot from their origin and are
        // siblings (not children) of the root dataset, so a plain recursive unmount
        // of the root dataset leaves sibling clones mounted and causes ZFS to report
        // the key as "busy".  We therefore find all datasets pool-wide that list this
        // dataset as their encryptionroot and unmount them individually.
        $poolName = explode('/', $params['name'])[0];
        $toUnmount = [];
        if (self::zfsTryExec(sprintf(
            'zfs get -H -t filesystem,volume -r -o name,value encryptionroot %s 2>/dev/null',
            escapeshellarg($poolName)
        ), $encRootLines)) {
            foreach ($encRootLines as $line) {
                $parts = explode("\t", trim($line));
                if (count($parts) === 2 && $parts[1] === $params['name']) {
                    $toUnmount[] = $parts[0];
                }
            }
        }
        // Fall back to the target dataset itself if the pool scan found nothing.
        if (empty($toUnmount)) {
            $toUnmount[] = $params['name'];
        }
        // Unmount deepest paths first so that child datasets are always
        // unmounted before their parents.  ZFS refuses to unmount a parent
        // while a mounted child is still present; reversing the list
        // (which is in hierarchical/alphabetical order from zfs get -r)
        // guarantees children come first.
        rsort($toUnmount);
        // Force-unmount each affected dataset; collect mountpoints for the lazy
        // umount fallback (needed when a process holds open file handles).
        $mountpoints = [];
        foreach ($toUnmount as $ds) {
            self::zfsTryExec(sprintf(
                'zfs list -H -t filesystem -o mountpoint %s 2>/dev/null',
                escapeshellarg($ds)
            ), $mpLines);
            foreach (($mpLines ?? []) as $mp) {
                $mp = trim($mp);
                if ($mp !== '' && !in_array($mp, ['none', 'legacy', '-'])) {
                    $mountpoints[] = $mp;
                }
            }
            self::zfsTryExec(sprintf('zfs unmount -f %s 2>&1', escapeshellarg($ds)));
        }
        foreach ($mountpoints as $mp) {
            self::zfsTryExec(sprintf('umount -l %s 2>/dev/null', escapeshellarg($mp)));
        }
        $cmd = sprintf('zfs unload-key -r %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
        // The datasets are now unmounted and the key is unavailable — safe to
        // write omvzfsplugin:uuid.  On OpenZFS 2.4.x-pve1 this call is unsafe
        // on mounted/key-loaded encrypted datasets (physically unmounts them),
        // so we defer the write to here where the dataset is locked.
        sort($toUnmount);
        foreach ($toUnmount as $ds) {
            self::addOMVMntEntForDataset($context, $ds);
        }
        return ['status' => 'unloaded'];
    }

    public function changeEncryptionKey($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.changeencryptionkey');
        // Try to load the old key first (may already be loaded; ignore failure).
        try {
            self::execWithStdin('zfs load-key ' . escapeshellarg($params['name']), $params['oldkey']);
        } catch (\Exception $e) {}
        self::execWithStdin('zfs change-key ' . escapeshellarg($params['name']), $params['newkey']);
        return ['status' => 'changed'];
    }

    public function getEncryptionStatus($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.encryptionstatus');
        $status = self::zfsGetEncryptionStatus($params['name']);
        return self::flattenEncryptionStatus($params['name'], $status);
    }

    public function setAutoUnlock($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.setautounlock');
        $name       = $params['name'];
        $keyloctype = $params['keyloctype'];
        if ($keyloctype === 'remote') {
            $remoteurl = $params['remoteurl'] ?? '';
            $insecure  = !empty($params['insecure']);
            if (empty($remoteurl) || strpos($remoteurl, 'https://') !== 0) {
                throw new \OMV\Exception('Remote URL must start with https://');
            }
            // Remove any local keyfile if switching from local to remote.
            $keyfile = self::getKeyfilePath($name);
            if (file_exists($keyfile)) {
                unlink($keyfile);
            }
            // Write remote metadata with restricted permissions.
            $metafile = self::getRemoteKeyMetaPath($name);
            $metadir  = dirname($metafile);
            if (!is_dir($metadir)) {
                mkdir($metadir, 0700, true);
            }
            $meta = ['dataset' => $name, 'url' => $remoteurl, 'insecure' => $insecure];
            $decryptPassphrase = $params['decrypt_passphrase'] ?? '';
            if (!empty($decryptPassphrase)) {
                $meta['decrypt_passphrase'] = $decryptPassphrase;
            }
            $networkTimeout = intval($params['network_timeout'] ?? 60);
            if ($networkTimeout < 10) $networkTimeout = 10;
            if ($networkTimeout > 600) $networkTimeout = 600;
            $meta['network_timeout'] = $networkTimeout;
            file_put_contents($metafile, json_encode($meta, JSON_PRETTY_PRINT));
            chmod($metafile, 0600);
            // keylocation=prompt — the zfs-remote-unlock service handles key loading at boot.
            self::zfsExec(sprintf('zfs set keylocation=prompt %s 2>&1', escapeshellarg($name)), $out, $res);
            self::manageRemoteUnlockService();
        } else {
            $keyfile = self::getKeyfilePath($name);
            $keydir  = dirname($keyfile);
            if (!is_dir($keydir)) {
                if (!mkdir($keydir, 0700, true)) {
                    throw new \OMV\Exception(sprintf('Failed to create key directory: %s', $keydir));
                }
            }
            if (file_put_contents($keyfile, $params['key']) === false) {
                throw new \OMV\Exception(sprintf('Failed to write keyfile: %s', $keyfile));
            }
            chmod($keyfile, 0600);
            // Remove any leftover remote metadata if switching from remote to local.
            $metafile = self::getRemoteKeyMetaPath($name);
            if (file_exists($metafile)) {
                unlink($metafile);
            }
            self::zfsExec(sprintf('zfs set keylocation=%s %s 2>&1',
                escapeshellarg('file://' . $keyfile), escapeshellarg($name)), $out, $res);
            self::manageRemoteUnlockService();
        }
        // Unmask the zfs-load-key@ unit for this dataset in case it was masked
        // by a prior removeAutoUnlock call, so auto-unlock takes effect at boot.
        $instanceOut = [];
        exec(sprintf('systemd-escape -p %s 2>/dev/null', escapeshellarg($name)), $instanceOut);
        $instance = trim(implode('', $instanceOut));
        if ($instance !== '') {
            self::zfsTryExec(sprintf('systemctl unmask %s 2>/dev/null',
                escapeshellarg('zfs-load-key@' . $instance . '.service')));
        }
        self::zfsTryExec('systemctl daemon-reload 2>/dev/null');
        self::updateZfsListCache(explode('/', $name)[0]);
        return ['autounlock' => true];
    }

    public function removeAutoUnlock($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.removeautounlock');
        $name = $params['name'];
        // Reset keylocation to prompt.
        self::zfsExec(sprintf('zfs set keylocation=prompt %s 2>&1', escapeshellarg($name)), $out, $res);
        // Remove local keyfile if present.
        $keyfile = self::getKeyfilePath($name);
        if (file_exists($keyfile)) {
            unlink($keyfile);
        }
        // Remove remote metadata if present.
        $metafile = self::getRemoteKeyMetaPath($name);
        if (file_exists($metafile)) {
            unlink($metafile);
        }
        self::manageRemoteUnlockService();
        // Mask the zfs-load-key@ unit for this dataset so it does not prompt
        // for a passphrase at boot (causing a hang) now that auto-unlock is
        // disabled.  systemctl mask creates a /dev/null symlink in
        // /etc/systemd/system/ which takes precedence over the generator's
        // output in /run/systemd/generator/ at next boot.
        $instanceOut = [];
        exec(sprintf('systemd-escape -p %s 2>/dev/null', escapeshellarg($name)), $instanceOut);
        $instance = trim(implode('', $instanceOut));
        if ($instance !== '') {
            self::zfsTryExec(sprintf('systemctl mask %s 2>/dev/null',
                escapeshellarg('zfs-load-key@' . $instance . '.service')));
        }
        self::zfsTryExec('systemctl daemon-reload 2>/dev/null');
        self::updateZfsListCache(explode('/', $name)[0]);
        return ['autounlock' => false, 'keylocation' => 'prompt'];
    }

    public function encryptRemoteKey($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.encryptremotekey');
        $descriptors = [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']];
        $cmd = sprintf(
            'openssl enc -aes-256-cbc -pbkdf2 -base64 -pass pass:%s',
            escapeshellarg($params['passphrase'])
        );
        $process = proc_open($cmd, $descriptors, $pipes);
        if (!is_resource($process)) {
            throw new \OMV\Exception('Failed to start openssl process.');
        }
        fwrite($pipes[0], $params['key']);
        fclose($pipes[0]);
        $encrypted = stream_get_contents($pipes[1]);
        $stderr    = stream_get_contents($pipes[2]);
        fclose($pipes[1]);
        fclose($pipes[2]);
        $rc = proc_close($process);
        if ($rc !== 0) {
            throw new \OMV\Exception('openssl enc failed: ' . trim($stderr));
        }
        return ['encrypted' => trim($encrypted)];
    }

    // -------------------------------------------------------------------------
    // Private helpers
    // -------------------------------------------------------------------------

    /**
     * Execute a shell command via OMV\System\Process, throwing on failure.
     */
    private static function zfsExec(string $cmd, &$out = null, &$res = null): void
    {
        if (file_exists('/var/log/zfs_commands.log')) {
            file_put_contents('/var/log/zfs_commands.log', $cmd . PHP_EOL, FILE_APPEND);
        }
        $process = new \OMV\System\Process($cmd);
        $process->execute($out, $res);
    }

    /**
     * Execute a shell command, returning false instead of throwing on failure.
     */
    private static function zfsTryExec(string $cmd, &$out = null, &$res = null): bool
    {
        try {
            self::zfsExec($cmd, $out, $res);
            return true;
        } catch (\Exception $e) {
            return false;
        }
    }

    /**
     * Execute a command that reads a secret from stdin (e.g. zfs load-key).
     */
    private static function execWithStdin(string $cmd, string $input): void
    {
        $descriptors = [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']];
        $process     = proc_open($cmd, $descriptors, $pipes);
        if (!is_resource($process)) {
            throw new \OMV\Exception(sprintf('Failed to start process: %s', $cmd));
        }
        fwrite($pipes[0], $input . "\n");
        fclose($pipes[0]);
        $stdout = stream_get_contents($pipes[1]);
        $stderr = stream_get_contents($pipes[2]);
        fclose($pipes[1]);
        fclose($pipes[2]);
        $result = proc_close($process);
        if ($result !== 0) {
            throw new \OMV\Exception('Command failed: ' . trim($stderr ?: $stdout));
        }
    }

    /**
     * Return the first absolute device path found under the 'logs' section,
     * or null if no log devices exist.
     */
    private static function findFirstLogDevice(string $pool): ?string
    {
        return self::findFirstClassDevice($pool, 'logs');
    }

    /**
     * Return the first absolute device path found under the 'special' section,
     * or null if no special devices exist.
     */
    private static function findFirstSpecialDevice(string $pool): ?string
    {
        return self::findFirstClassDevice($pool, 'special');
    }

    /**
     * Return the first absolute device path under a named class section
     * ('logs', 'special', 'l2cache', 'spares') using zpool status -j.
     *
     * The JSON output from 'zpool status -j' exposes class vdevs as top-level
     * keys on the pool object (e.g. $pool['logs'], $pool['special']) rather
     * than nesting them inside the main 'vdevs' tree.  The section key names
     * in JSON match the text-output section headers exactly for 'logs' and
     * 'special'; cache uses 'l2cache'.
     */
    private static function findFirstClassDevice(string $pool, string $section): ?string
    {
        $cmd = sprintf('zpool status -j %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return null;
        }
        $json = json_decode(implode('', $out), true);
        foreach ($json['pools'][$pool][$section] ?? [] as $vdev) {
            $path = self::firstVdevPath($vdev);
            if ($path !== null) {
                return $path;
            }
        }
        return null;
    }

    /**
     * Recursively return the first 'path' found in a vdev tree, or null.
     */
    private static function firstVdevPath(array $vdev): ?string
    {
        if (!empty($vdev['path'])) {
            return $vdev['path'];
        }
        foreach ($vdev['vdevs'] ?? [] as $child) {
            $path = self::firstVdevPath($child);
            if ($path !== null) {
                return $path;
            }
        }
        return null;
    }

    /**
     * Build a zpool vdev command string, e.g. "mirror /dev/sda /dev/sdb".
     */
    private static function buildVdevString(string $type, array $disks): string
    {
        $diskStr = implode(' ', array_map('escapeshellarg', $disks));
        switch ($type) {
            case 'basic':  return $diskStr;
            case 'mirror': return 'mirror ' . $diskStr;
            case 'raidz1': return 'raidz1 ' . $diskStr;
            case 'raidz2': return 'raidz2 ' . $diskStr;
            case 'raidz3': return 'raidz3 ' . $diskStr;
            default:       throw new \OMV\Exception('Unknown vdev type: ' . $type);
        }
    }

    /**
     * Get all physical device paths from a pool via zpool status -j,
     * excluding devices with states in $excludeStates (e.g. ['INUSE','UNAVAIL','SPLIT']).
     */
    public static function zpoolGetAllDevices(string $poolName, array $excludeStates = []): array
    {
        $cmd = sprintf('zpool status -j %s 2>&1', escapeshellarg($poolName));
        self::zfsExec($cmd, $out, $res);
        $json    = json_decode(implode('', $out), true);
        $poolObj = $json['pools'][$poolName] ?? null;
        if ($poolObj === null) {
            return [];
        }
        $devices  = [];
        $sections = array_merge(
            array_values($poolObj['vdevs'][$poolName]['vdevs'] ?? []),
            array_values($poolObj['logs']    ?? []),
            array_values($poolObj['l2cache'] ?? []),
            array_values($poolObj['special'] ?? []),
            array_values($poolObj['spares']  ?? [])
        );
        foreach ($sections as $vdev) {
            self::collectLeafPaths($vdev, $excludeStates, $devices);
        }
        return $devices;
    }

    /**
     * Return all ZFS properties for a dataset as ['propname' => ['value'=>..., 'source'=>...]].
     */
    private static function zfsGetAllPropertiesRaw(string $name): array
    {
        $cmd = sprintf('zfs get -H all %s 2>&1', escapeshellarg($name));
        self::zfsExec($cmd, $out, $res);
        $properties = [];
        foreach ($out as $line) {
            $parts = preg_split('/\t+/', $line);
            if (count($parts) >= 4) {
                $properties[$parts[1]] = ['value' => $parts[2], 'source' => $parts[3]];
            }
        }
        return $properties;
    }

    private static function zpoolGetAllPropertiesRaw(string $name): array
    {
        $cmd = sprintf('zpool get -H all %s 2>&1', escapeshellarg($name));
        self::zfsExec($cmd, $out, $res);
        $properties = [];
        foreach ($out as $line) {
            $parts = preg_split('/\t+/', $line);
            if (count($parts) >= 4) {
                $properties[$parts[1]] = ['value' => $parts[2], 'source' => $parts[3]];
            }
        }
        return $properties;
    }

    /**
     * Return true if the named ZFS filesystem exists.
     */
    private static function zfsFilesystemExists(string $name): bool
    {
        return self::zfsTryExec(
            sprintf('zfs list -H -t filesystem %s 2>&1', escapeshellarg($name)));
    }

    /**
     * Mark one or more OMV config modules dirty so they are applied the next
     * time the user clicks Apply (or on the next scheduled config flush).
     * Prefer this over Config::applyChanges for modules whose output is not
     * required for the current operation to succeed (collectd, fstab, monit,
     * quota).  Safe to call from both instance and static methods.
     */
    private static function dirtyModules(array $modules): void
    {
        $mgr = \OMV\Engine\Module\Manager::getInstance();
        foreach ($modules as $module) {
            $mgr->setModuleDirty($module);
        }
    }

    /**
     * Classify a top-level vdev token as mirror / raidz1 / raidz2 / raidz3 / stripe.
     */
    private static function vdevTypeFromToken(string $token): string
    {
        if (preg_match('/^mirror-\d+$/', $token))  return 'mirror';
        if (preg_match('/^raidz3-\d+$/', $token))  return 'raidz3';
        if (preg_match('/^raidz2-\d+$/', $token))  return 'raidz2';
        if (preg_match('/^raidz1?-\d+$/', $token)) return 'raidz1';
        if (str_starts_with($token, '/'))           return 'stripe';
        return 'unknown';
    }

    /**
     * Convert a JSON vdev_type + name to a canonical vdev type string.
     * Uses the vdev name to distinguish raidz levels when vdev_type is generic 'raidz'.
     */
    private static function vdevTypeFromJson(string $vdevType, string $name): string
    {
        if ($vdevType === 'mirror') return 'mirror';
        if ($vdevType === 'disk')   return 'stripe';
        if (str_starts_with($vdevType, 'raidz') || str_starts_with($name, 'raidz')) {
            if (str_contains($name, 'raidz3')) return 'raidz3';
            if (str_contains($name, 'raidz2')) return 'raidz2';
            return 'raidz1';
        }
        return 'unknown';
    }

    /**
     * Recursively collect device records from a vdev tree.
     * Used by getPoolDevices and getAttachAnchors.
     *
     * @param array  $vdev       The vdev node from zpool status -j JSON.
     * @param string $groupName  Parent vdev name ('' for top-level stripe disks).
     * @param array  &$out       Accumulator for device records.
     */
    private static function collectDevicesRecursive(array $vdev, string $groupName, array &$out): void
    {
        if (!empty($vdev['path'])) {
            $state = strtoupper($vdev['state'] ?? 'UNKNOWN');
            $label = $groupName
                ? $vdev['path'] . '  [' . $groupName . '] [' . $state . ']'
                : $vdev['path'] . '  [stripe] [' . $state . ']';
            $out[] = [
                'devicefile' => $vdev['path'],
                'state'      => $state,
                'vdev'       => $groupName,
                'label'      => $label,
            ];
            return;
        }
        $name = $vdev['name'] ?? $groupName;
        foreach ($vdev['vdevs'] ?? [] as $child) {
            self::collectDevicesRecursive($child, $name ?: $groupName, $out);
        }
    }

    /**
     * Recursively collect leaf device paths from a vdev tree.
     * Used by zpoolGetAllDevices.
     *
     * @param array    $vdev           The vdev node from zpool status -j JSON.
     * @param string[] $excludeStates  Uppercase state strings to skip (e.g. 'UNAVAIL').
     * @param string[] &$out           Accumulator for device paths.
     */
    private static function collectLeafPaths(array $vdev, array $excludeStates, array &$out): void
    {
        if (!empty($vdev['path'])) {
            $state = strtoupper($vdev['state'] ?? '');
            if (empty($excludeStates) || !in_array($state, $excludeStates)) {
                $out[] = $vdev['path'];
            }
            return;
        }
        foreach ($vdev['vdevs'] ?? [] as $child) {
            self::collectLeafPaths($child, $excludeStates, $out);
        }
    }

    /**
     * Resolve a /dev/sdX path to its /dev/disk/by-id symlink.
     */
    private static function getDiskId(string $disk): string
    {
        preg_match('/^\/dev\/(.*)$/', $disk, $id);
        if (empty($id[1])) {
            return $disk;
        }
        $cmd = "ls -la /dev/disk/by-id | grep '" . preg_quote($id[1], "'") . "\$'";
        self::zfsExec($cmd, $out, $res);
        if (!empty($out)) {
            $cols = preg_split('/[\s]+/', $out[0]);
            if (count($cols) >= 3) {
                return '/dev/disk/by-id/' . $cols[count($cols) - 3];
            }
        }
        return $disk;
    }

    /**
     * Resolve a /dev/sdX path to its /dev/disk/by-path symlink.
     */
    private static function getDiskPath(string $disk): string
    {
        preg_match('/^\/dev\/(.*)$/', $disk, $id);
        if (empty($id[1])) {
            return $disk;
        }
        $cmd = "ls -la /dev/disk/by-path | grep '" . preg_quote($id[1], "'") . "\$'";
        self::zfsExec($cmd, $out, $res);
        if (!empty($out)) {
            $cols = preg_split('/[\s]+/', $out[0]);
            if (count($cols) >= 3) {
                return '/dev/disk/by-path/' . $cols[count($cols) - 3];
            }
        }
        return $disk;
    }

    /**
     * Extract the pool name from a dataset path (first component before '/').
     */
    private static function getPoolFromFsname(string $fsname): string
    {
        $pos = strpos($fsname, '/');
        return ($pos === false) ? $fsname : substr($fsname, 0, $pos);
    }

    /**
     * Return true if the named pool is currently imported.
     */
    private static function isPoolImported(string $name): bool
    {
        return self::zfsTryExec(sprintf('zpool status %s', escapeshellarg($name)));
    }

    /**
     * Write the OMV mntent UUID into a ZFS dataset user property.
     */
    private static function setMntentProperty(string $uuid, string $fsname): bool
    {
        if (!self::isPoolImported(self::getPoolFromFsname($fsname))) {
            return false;
        }
        $cmd = sprintf('zfs set omvzfsplugin:uuid=%s %s 2>&1',
            escapeshellarg($uuid), escapeshellarg($fsname));
        $propOk = self::zfsTryExec($cmd);
        // On some kernel versions (e.g. PVE 6.17/OpenZFS), setting a user
        // property on a mounted encrypted dataset clears ZFS's internal
        // mounted-state tracking without actually unmounting the filesystem.
        // Re-mounting is a no-op if already properly tracked, and safely
        // re-establishes tracking if it was cleared.
        $mountOk = self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($fsname)));
        return $propOk && $mountOk;
    }

    /**
     * Return true if any shared folder references the fstab entry for $mountpoint.
     */
    private static function checkOMVShares($context, string $mountpoint): bool
    {
        if (empty($mountpoint) || $mountpoint === '-') {
            return false;
        }
        $object = \OMV\Rpc\Rpc::call('FsTab', 'getByDir', ['dir' => $mountpoint], $context);
        if (!$object) {
            return false;
        }
        $uuid   = $object['uuid'];
        $shares = \OMV\Rpc\Rpc::call('ShareMgmt', 'enumerateSharedFolders', [], $context);
        foreach ($shares as $share) {
            if ($share['mntentref'] === $uuid) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return the names of all OMV shared folders linked to the fstab entry for
     * $mountpoint, or an empty array if none.
     */
    private static function getLinkedSharedFolderNames($context, string $mountpoint): array
    {
        try {
            $object = \OMV\Rpc\Rpc::call('FsTab', 'getByDir', ['dir' => $mountpoint], $context);
            if (!$object) {
                return [];
            }
            $uuid   = $object['uuid'];
            $shares = \OMV\Rpc\Rpc::call('ShareMgmt', 'enumerateSharedFolders', [], $context);
            $names  = [];
            foreach ($shares as $share) {
                if ($share['mntentref'] === $uuid) {
                    $names[] = $share['name'];
                }
            }
            return $names;
        } catch (\Exception $e) {
            return [];
        }
    }

    /**
     * Return a list of "procname (PID N)" strings for all processes currently
     * holding the given mountpoint open, via fuser -m.
     */
    private static function getMountBusyProcesses(string $mountpoint): array
    {
        $output = [];
        @exec('fuser -m ' . escapeshellarg($mountpoint) . ' 2>&1', $output);
        $pids = [];
        foreach ($output as $line) {
            foreach (preg_split('/\s+/', $line) as $tok) {
                // PIDs may carry a trailing access-type letter (e, r, f, c, m, s).
                if (preg_match('/^(\d+)[a-z]*$/', $tok, $m)) {
                    $pid = (int)$m[1];
                    if ($pid > 1) {
                        $pids[] = $pid;
                    }
                }
            }
        }
        $procs = [];
        foreach (array_unique($pids) as $pid) {
            $comm = @file_get_contents("/proc/{$pid}/comm");
            if ($comm !== false) {
                $procs[] = trim($comm) . ' (PID ' . $pid . ')';
            }
        }
        return $procs;
    }

    /**
     * Build a human-readable error message when 'zfs rename' fails because the
     * dataset is busy.  Identifies linked shared folders and holding processes.
     */
    private static function buildBusyRenameError($context, string $dataset): string
    {
        // Resolve the dataset's mountpoint.
        $mountpoint = null;
        if (self::zfsTryExec(
            sprintf('zfs get -H -o value mountpoint %s 2>&1', escapeshellarg($dataset)),
            $mpOut
        )) {
            $mp = trim(implode('', $mpOut));
            if (!in_array($mp, ['-', 'none', 'legacy', ''], true)) {
                $mountpoint = $mp;
            }
        }

        $reasons = [];
        if ($mountpoint !== null) {
            $shares = self::getLinkedSharedFolderNames($context, $mountpoint);
            if (!empty($shares)) {
                $quoted   = array_map(fn($n) => '"' . $n . '"', $shares);
                $reasons[] = 'it is linked to shared folder(s) ' . implode(', ', $quoted)
                           . ' — disable the service shares or remove the shared folder(s) before renaming';
            }
            $procs = self::getMountBusyProcesses($mountpoint);
            if (!empty($procs)) {
                $reasons[] = 'it is in use by process(es): ' . implode(', ', $procs)
                           . ' — stop them and retry';
            }
        }

        if (empty($reasons)) {
            return sprintf(
                'Cannot rename "%s": the dataset is busy. '
                . 'Ensure no processes are accessing its mountpoint and no '
                . 'services are sharing it, then try again.',
                $dataset
            );
        }
        return sprintf(
            'Cannot rename "%s": the dataset is busy because %s.',
            $dataset,
            implode('; and ', $reasons)
        );
    }

    /**
     * Delete snapshot jobs referencing $dataset (exact match or child of $dataset).
     * Delete scrub jobs referencing $pool (exact match).
     */
    private static function cleanupJobsForDataset(string $dataset): void
    {
        $db = \OMV\Config\Database::getInstance();
        $prefix = $dataset . '/';
        foreach ($db->get('conf.service.zfs.snapshotjob') as $obj) {
            $d = $obj->get('dataset');
            if ($d === $dataset || str_starts_with($d, $prefix)) {
                $db->delete($obj);
            }
        }
    }

    private static function cleanupJobsForPool(string $pool): void
    {
        $db = \OMV\Config\Database::getInstance();
        $prefix = $pool . '/';
        foreach ($db->get('conf.service.zfs.snapshotjob') as $obj) {
            $d = $obj->get('dataset');
            if ($d === $pool || str_starts_with($d, $prefix)) {
                $db->delete($obj);
            }
        }
        foreach ($db->get('conf.service.zfs.scrubjob') as $obj) {
            if ($obj->get('pool') === $pool) {
                $db->delete($obj);
            }
        }
    }

    /**
     * Remove the fstab entry for $mountpoint and destroy the ZFS filesystem.
     */
    private static function deleteOMVMntEnt($context, string $name, string $mountpoint): void
    {
        // A dataset may legitimately have no mntent entry (e.g. a promoted clone,
        // a dataset created via CLI, or one imported from another system).  Skip
        // the FsTab deletion gracefully rather than blocking the destroy operation.
        $object = \OMV\Rpc\Rpc::call('FsTab', 'getByDir', ['dir' => $mountpoint], $context);
        if ($object && $object['type'] === 'zfs' && $object['dir'] === $mountpoint) {
            \OMV\Rpc\Rpc::call('FsTab', 'delete', ['uuid' => $object['uuid']], $context);
        }
        if (self::zfsFilesystemExists($name)) {
            // Refuse if any snapshot of this dataset has dependent clones — zfs destroy
            // would fail cryptically and the caller needs to know what to delete first.
            $dependents = self::getSnapshotDependentClones($name);
            if (!empty($dependents)) {
                throw new \OMV\Exception(
                    "Cannot delete '%s' — it has snapshots that are the origin of: %s. "
                    . "Delete those datasets first, or use Promote on one of them to restore "
                    . "the original clone relationship.",
                    $name, implode(', ', $dependents)
                );
            }
            // Use -r so any orphaned snapshots are removed along with the dataset.
            self::zfsExec(sprintf('zfs destroy -r %s 2>&1', escapeshellarg($name)), $out, $res);
        }
    }

    /**
     * Update fstab dir entries when a dataset's mountpoint property changes.
     */
    private static function relocateOMVMntEnt($context, string $name): void
    {
        $db  = \OMV\Config\Database::getInstance();
        $cmd = sprintf('zfs list -H -p -r -t filesystem -o name %s 2>&1', escapeshellarg($name));
        self::zfsExec($cmd, $out, $res);
        foreach ($out as $line) {
            $child = trim($line);
            if (empty($child)) {
                continue;
            }
            $cmd2   = sprintf('zfs get -H -p mountpoint %s 2>&1', escapeshellarg($child));
            self::zfsExec($cmd2, $mout, $mres);
            $parts      = preg_split('/\t+/', $mout[0] ?? '');
            $mountpoint = $parts[2] ?? '';
            $object     = \OMV\Rpc\Rpc::call('FsTab', 'getByFsName', ['fsname' => $child], $context);
            if ($object) {
                $config = $db->get('conf.system.filesystem.mountpoint', $object['uuid']);
                $config->set('dir', $mountpoint);
                $db->set($config, true);
            }
        }
    }

    /**
     * Update shared folder mntentref from an old UUID to a new one.
     */
    private static function fixOMVSharedFolders(
        string $oldUuid,
        string $newUuid,
        array  $sfObjects,
        $context
    ): void {
        foreach ($sfObjects as $sf) {
            if ($sf['mntentref'] !== $oldUuid) {
                continue;
            }
            unset($sf['mntent'], $sf['_used'], $sf['privileges']);
            $sf['mntentref'] = $newUuid;
            \OMV\Rpc\Rpc::call('ShareMgmt', 'set', $sf, $context);
        }
    }

    /**
     * Add any missing ZFS datasets to the OMV fstab database and fix
     * shared folder UUID references when mntent UUIDs have changed.
     */
    private static function addOMVMntEntForDataset($context, string $fsname, bool $setProperty = true): void
    {
        // Get the actual mountpoint from ZFS.
        $cmd = sprintf('zfs get -H -o value mountpoint %s 2>&1', escapeshellarg($fsname));
        if (!self::zfsTryExec($cmd, $o, $r) || $r !== 0) {
            return;
        }
        $mountpoint = trim(implode('', $o));
        if (in_array($mountpoint, ['-', 'none', 'legacy', '/'])) {
            return;
        }
        // Update or skip if an mntent entry already exists for this dataset.
        $entries = \OMV\Rpc\Rpc::call('FsTab', 'enumerateEntries', [], $context);
        foreach ($entries as $entry) {
            if (isset($entry['fsname']) && $entry['fsname'] === $fsname) {
                if ($entry['dir'] !== $mountpoint) {
                    $db = \OMV\Config\Database::getInstance();
                    $config = $db->get('conf.system.filesystem.mountpoint', $entry['uuid']);
                    $config->set('dir', $mountpoint);
                    $db->set($config, true);
                }
                if ($setProperty) {
                    self::setMntentProperty($entry['uuid'], $fsname);
                }
                return;
            }
        }
        $uuid = \OMV\Environment::get('OMV_CONFIGOBJECT_NEW_UUID');
        \OMV\Rpc\Rpc::call('FsTab', 'set', [
            'uuid'   => $uuid,
            'fsname' => $fsname,
            'dir'    => $mountpoint,
            'type'   => 'zfs',
            'opts'   => '',
            'freq'   => 0,
            'passno' => 0,
        ], $context);
        if ($setProperty) {
            self::setMntentProperty($uuid, $fsname);
        }
    }

    private static function fixOMVMntEnt($context, bool $addMissing = true, bool $deleteStale = false): void
    {
        // Back up config.xml, keeping the 20 most recent copies.
        $src       = '/etc/openmediavault/config.xml';
        $dir       = '/etc/openmediavault/';
        $timestamp = date('Ymd-His');
        if (!@copy($src, $dir . 'config-zfs-' . $timestamp . '.xml')) {
            throw new \OMV\Exception(sprintf('Failed to backup %s', $src));
        }
        $files = glob($dir . 'config-*.xml', GLOB_NOSORT) ?: [];
        usort($files, function ($a, $b) { return filemtime($b) <=> filemtime($a); });
        foreach (array_slice($files, 20) as $file) {
            @unlink($file);
        }
        // Build list of mountable ZFS datasets from the system.
        $current = [];
        try {
            self::zfsExec('zfs list -H -p -o name,mountpoint', $out, $res);
            foreach ($out as $line) {
                $line = trim($line);
                if ($line === '') {
                    continue;
                }
                $parts = explode("\t", $line, 2);
                if (count($parts) !== 2) {
                    continue;
                }
                [$name, $mntpoint] = $parts;
                if (in_array($mntpoint, ['-', 'none', 'legacy', '/'])) {
                    continue;
                }
                $current[] = ['fsname' => $name, 'dir' => $mntpoint];
            }
        } catch (\Exception $e) {
            return;
        }
        // Get existing ZFS fstab entries.
        $prev = \OMV\Rpc\Rpc::call('FsTab', 'enumerateEntries', [], $context);
        $prev = array_values(array_filter($prev, function ($e) {
            return isset($e['type']) && $e['type'] === 'zfs';
        }));
        // Compare entries by fsname and dir.
        $compare = function ($a, $b) {
            $c = strcmp($a['fsname'], $b['fsname']);
            return $c !== 0 ? $c : strcmp($a['dir'], $b['dir']);
        };
        // Add entries that are in $current but missing from $prev.
        if ($addMissing) {
            foreach (array_udiff($current, $prev, $compare) as $object) {
                $uuid = \OMV\Environment::get('OMV_CONFIGOBJECT_NEW_UUID');
                \OMV\Rpc\Rpc::call('FsTab', 'set', [
                    'uuid'   => $uuid,
                    'fsname' => $object['fsname'],
                    'dir'    => $object['dir'],
                    'type'   => 'zfs',
                    'opts'   => '',
                    'freq'   => 0,
                    'passno' => 0,
                ], $context);
            }
        }
        // Delete entries that are in $prev but no longer in $current.
        if ($deleteStale) {
            foreach (array_udiff($prev, $current, $compare) as $object) {
                \OMV\Rpc\Rpc::call('FsTab', 'delete', ['uuid' => $object['uuid']], $context);
            }
        }
        // Refresh the list and fix any shared folder UUID mappings.
        // Only process entries whose dataset still exists on the system to
        // avoid errors when stale mntent entries have not been deleted.
        $currentFsnames = array_column($current, 'fsname');
        $zfsMntent = \OMV\Rpc\Rpc::call('FsTab', 'enumerateEntries', [], $context);
        $zfsMntent = array_values(array_filter($zfsMntent, function ($e) use ($currentFsnames) {
            return isset($e['type']) && $e['type'] === 'zfs'
                && in_array($e['fsname'], $currentFsnames);
        }));
        $sfObjects = \OMV\Rpc\Rpc::call('ShareMgmt', 'enumerateSharedFolders', [], $context);
        foreach ($zfsMntent as $dataset) {
            $fsname = $dataset['fsname'];
            $pool   = self::getPoolFromFsname($fsname);
            if (!self::isPoolImported($pool)) {
                continue;
            }
            // Read mountpoint, stored UUID, and keystatus from the ZFS dataset.
            $cmd = sprintf('zfs get -H -p mountpoint,omvzfsplugin:uuid,keystatus %s 2>&1',
                escapeshellarg($fsname));
            self::zfsExec($cmd, $o, $r);
            $props = [];
            foreach ($o as $line) {
                $parts = preg_split('/\t+/', $line);
                if (count($parts) >= 4) {
                    $props[$parts[1]] = ['value' => $parts[2], 'source' => $parts[3]];
                }
            }
            $mountpoint  = $props['mountpoint']['value'] ?? '';
            $oldUuidProp = $props['omvzfsplugin:uuid'] ?? null;
            $oldUuid     = ($oldUuidProp
                && $oldUuidProp['value'] !== ''
                && $oldUuidProp['source'] === 'local')
                ? $oldUuidProp['value']
                : null;
            $newUuid = $dataset['uuid'];
            if ($oldUuid !== $newUuid) {
                // UUID mismatch (or not yet set): fix any shared-folder references
                // that point at the old UUID, then write the correct UUID to ZFS.
                // We intentionally skip setMntentProperty when the UUID is already
                // correct: every call triggers 'zfs set omvzfsplugin:uuid=...' which,
                // on some kernel versions (e.g. PVE 6.17/OpenZFS), physically unmounts
                // a mounted encrypted dataset or puts an unmounted one into a ghost
                // mount state.  We also skip it when the encryption key is loaded
                // (keystatus=available) for the same reason — the write is only safe
                // when the dataset is locked (keystatus=unavailable or not encrypted).
                // The UUID is written by unloadEncryptionKey after locking, ensuring
                // it is always current for the next pool import.
                $keystatus = $props['keystatus']['value'] ?? '-';
                if ($oldUuid) {
                    $matchingSfs = array_filter($sfObjects, function ($sf) use ($mountpoint) {
                        return isset($sf['mntent']['dir'])
                            && rtrim($sf['mntent']['dir'], '/') === rtrim($mountpoint, '/');
                    });
                    if (!empty($matchingSfs)) {
                        self::fixOMVSharedFolders($oldUuid, $newUuid, array_values($matchingSfs), $context);
                    }
                }
                if ($keystatus !== 'available') {
                    self::setMntentProperty($newUuid, $fsname);
                }
            }
        }
    }

    /**
     * Return a flat array of all pools, filesystems, and volumes with their stats.
     */
    private static function getZFSFlatArray(): array
    {
        $prefix  = 'root/pool-';
        $objects = [];
        // Fetch pool status for all pools in a single zpool status -j call.
        $poolStatus  = self::zpoolFetchAllStatus();
        // Fetch physical SIZE/ALLOC/FREE for all pools.
        $poolSizes   = self::zpoolFetchAllSizes();
        // Build a set of ZFS dataset fsnames that have shared folders.
        $sharedFsnames = self::buildSharedFsnames();
        // Fetch encryption properties for all datasets in one call.
        $encMap = [];
        self::zfsTryExec(
            'zfs get -H -p encryption,encryptionroot,keystatus,keylocation -t filesystem,volume 2>&1',
            $encOut
        );
        foreach ($encOut as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 3) {
                continue;
            }
            $encMap[$parts[0]][$parts[1]] = $parts[2];
        }
        // Build a set of ZFS dataset names that are actually mounted in the
        // kernel via /proc/mounts.  On OpenZFS 2.4.x-pve1, writing a ZFS user
        // property (omvzfsplugin:uuid) on a mounted encrypted dataset clears
        // the module's internal mounted-state tracking, so zfs list reports
        // mounted=no even though the dataset is fully accessible.  Using
        // /proc/mounts as the authoritative source avoids this false display.
        $procMountedSet = [];
        foreach (@file('/proc/mounts', FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) ?: [] as $mline) {
            $mparts = explode(' ', $mline);
            if (count($mparts) >= 3 && $mparts[2] === 'zfs') {
                $procMountedSet[$mparts[0]] = true;
            }
        }
        // One zfs list call returns all the data we need for sizing/type/origin.
        $cmd = 'zfs list -H -p -t filesystem,volume '
             . '-o name,type,used,available,mountpoint,origin,volsize,logicalused,refreservation,quota,compression,compressratio,refer,mounted 2>&1';
        self::zfsExec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 14) {
                continue;
            }
            [$path, $type, $used, $available, $mountpoint,
             $origin, $volsize, $logicalused, $refreservation, $quota,
             $compression, $compressratio, $refer, $mounted] = $parts;
            $isPool       = ($type === 'filesystem' && strpos($path, '/') === false);
            $isFilesystem = ($type === 'filesystem' && !$isPool);
            $isVolume     = ($type === 'volume');
            $enc             = strtoupper($encMap[$path]['encryption'] ?? 'off');
            $encRoot         = $encMap[$path]['encryptionroot'] ?? '-';
            $keystatus       = $encMap[$path]['keystatus']   ?? '-';
            $keylocation     = $encMap[$path]['keylocation'] ?? 'none';
            $encrypted       = ($enc !== 'OFF');
            $isEncryptionRoot = $encrypted && $encRoot === $path;
            $autounlock   = $encrypted && (
                (strpos($keylocation, 'file://') === 0 && file_exists(self::getKeyfilePath($path))) ||
                file_exists(self::getRemoteKeyMetaPath($path))
            );
            if ($isPool) {
                $pz         = $poolSizes[$path] ?? null;
                $size       = $pz ? $pz['size']  : 0;
                $usedBytes  = $pz ? $pz['alloc'] : 0;
                $availBytes = $pz ? $pz['free']  : 0;
                $ps         = $poolStatus[$path] ?? [];
                $objects[] = [
                    'id'          => $prefix . $path,
                    'parentid'    => 'root',
                    'name'        => $path,
                    'type'        => 'Pool',
                    'icon'        => 'images/raid.png',
                    'expanded'    => true,
                    'path'        => $path,
                    'origin'      => 'n/a',
                    'size'        => $size,
                    'used'        => $usedBytes,
                    'usedpercent' => $size > 0 ? ($usedBytes / $size * 100) : 0,
                    'available'   => $availBytes,
                    'mountpoint'  => ($mountpoint !== '-') ? $mountpoint : '',
                    'lastscrub'     => $ps['lastscrub']     ?? 'Unknown',
                    'state'         => $ps['state']         ?? 'UNKNOWN',
                    'status'        => $ps['status']        ?? 'Unknown',
                    'scrubstate'    => $ps['scrubstate']    ?? 'unknown',
                    'scrubactive'   => $ps['scrubactive']   ?? false,
                    'removalstate'  => $ps['removalstate']  ?? 'none',
                    'removalactive' => $ps['removalactive'] ?? false,
                    'encryption'    => $enc,
                    'keystatus'     => $keystatus,
                    'autounlock'    => $autounlock,
                    'hasshares'     => isset($sharedFsnames[$path]),
                    'fragmentation' => $pz ? $pz['frag']     : 0,
                    'dedup'         => $pz ? $pz['dedup']    : '1.00x',
                    'capacity'      => $pz ? $pz['capacity'] : 0,
                ];
                // Also emit the pool root as a Filesystem entry so it appears
                // on the Datasets tab and its filesystem properties are accessible.
                $fsUsed  = ($used !== '-') ? (int)$used : 0;
                $fsAvail = ($available !== '-') ? (int)$available : 0;
                $objects[] = [
                    'id'            => 'root/poolfs-' . $path,
                    'parentid'      => 'root',
                    'name'          => $path,
                    'type'          => 'Filesystem',
                    'icon'          => 'images/filesystem.png',
                    'path'          => $path,
                    'pool'          => $path,
                    'expanded'      => true,
                    'origin'        => 'n/a',
                    'size'          => null,
                    'used'          => $fsUsed,
                    'usedpercent'   => 0,
                    'available'     => $fsAvail,
                    'mountpoint'    => ($mountpoint !== '-') ? $mountpoint : '',
                    'lastscrub'     => 'n/a',
                    'state'         => 'n/a',
                    'status'        => 'n/a',
                    'scrubstate'    => 'n/a',
                    'scrubactive'   => false,
                    'encryption'       => $enc,
                    'keystatus'        => $keystatus,
                    'autounlock'       => $autounlock,
                    'isencryptionroot' => $isEncryptionRoot,
                    'hasshares'        => isset($sharedFsnames[$path]),
                    'compression'      => ($compression !== '-') ? $compression : 'off',
                    'compressratio'    => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'            => ($refer !== '-') ? (int)$refer : 0,
                    'mounted'          => (trim($mounted) === 'yes') || isset($procMountedSet[$path]),
                    'poolroot'         => true,
                ];
            } elseif ($isFilesystem) {
                preg_match('/(.*)\/(.*)$/', $path, $result);
                $usedBytes   = ($used !== '-') ? (int)$used : 0;
                $availRaw    = ($available !== '-') ? (int)$available : 0;
                $quotaBytes  = ($quota !== '-' && $quota !== 'none' && (int)$quota > 0) ? (int)$quota : 0;
                // A filesystem has no fixed size unless a quota is set.
                $size        = $quotaBytes > 0 ? $quotaBytes : null;
                // AVAIL from zfs list is the actual space available to this dataset
                // (pool free space, or quota remaining if a quota is set).
                $availBytes  = $availRaw;
                $objects[] = [
                    'id'          => $prefix . $path,
                    'parentid'    => $prefix . $result[1],
                    'name'        => $result[2],
                    'type'        => 'Filesystem',
                    'icon'        => 'images/filesystem.png',
                    'path'        => $path,
                    'pool'        => explode('/', $path)[0],
                    'expanded'    => true,
                    'origin'      => ($origin !== '-' && $origin !== '') ? $origin : 'n/a',
                    'size'        => $size,
                    'used'        => $usedBytes,
                    'usedpercent' => $size > 0 ? ($usedBytes / $size * 100) : 0,
                    'available'   => $availBytes,
                    'mountpoint'  => ($mountpoint !== '-') ? $mountpoint : '',
                    'lastscrub'   => 'n/a',
                    'state'       => 'n/a',
                    'status'      => 'n/a',
                    'scrubstate'  => 'n/a',
                    'scrubactive' => false,
                    'encryption'       => $enc,
                    'keystatus'        => $keystatus,
                    'autounlock'       => $autounlock,
                    'isencryptionroot' => $isEncryptionRoot,
                    'hasshares'        => isset($sharedFsnames[$path]),
                    'compression'      => ($compression !== '-') ? $compression : 'off',
                    'compressratio'    => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'            => ($refer !== '-') ? (int)$refer : 0,
                    'mounted'          => (trim($mounted) === 'yes') || isset($procMountedSet[$path]),
                    'poolroot'         => false,
                ];
            } elseif ($isVolume) {
                preg_match('/(.*)\/(.*)$/', $path, $result);
                $volSizeBytes = ($volsize !== '-') ? (int)$volsize : 0;
                $logUsedBytes = ($logicalused !== '-') ? (int)$logicalused : 0;
                $isThin       = (trim($refreservation) === 'none');
                $objects[] = [
                    'id'          => $prefix . $path,
                    'parentid'    => $prefix . $result[1],
                    'name'        => $result[2],
                    'type'        => 'Volume',
                    'icon'        => $isThin ? 'images/zfs_thinvol.png' : 'images/save.png',
                    'path'        => $path,
                    'pool'        => explode('/', $path)[0],
                    'expanded'    => true,
                    'origin'      => ($origin !== '-' && $origin !== '') ? $origin : 'n/a',
                    'size'        => $volSizeBytes,
                    'used'        => $logUsedBytes,
                    'usedpercent' => $volSizeBytes > 0 ? ($logUsedBytes / $volSizeBytes * 100) : 0,
                    'available'   => $volSizeBytes - $logUsedBytes,
                    'mountpoint'  => 'n/a',
                    'lastscrub'   => 'n/a',
                    'state'       => 'n/a',
                    'status'      => 'n/a',
                    'scrubstate'  => 'n/a',
                    'scrubactive' => false,
                    'encryption'       => $enc,
                    'keystatus'        => $keystatus,
                    'autounlock'       => $autounlock,
                    'isencryptionroot' => $isEncryptionRoot,
                    'hasshares'        => false,
                    'compression'      => ($compression !== '-') ? $compression : 'off',
                    'compressratio'    => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'            => ($refer !== '-') ? (int)$refer : 0,
                    'mounted'          => false,
                ];
            }
        }
        // Mark objects that have at least one child dataset, volume, or snapshot.
        $parentPaths = [];
        foreach ($objects as $obj) {
            $path = $obj['path'];
            // Child filesystem/volume: parent is everything before the last /.
            $slash = strrpos($path, '/');
            if ($slash !== false) {
                $parentPaths[substr($path, 0, $slash)] = true;
            }
            // Snapshot: parent is everything before the @.
            $at = strpos($path, '@');
            if ($at !== false) {
                $parentPaths[substr($path, 0, $at)] = true;
            }
        }
        foreach ($objects as &$obj) {
            $obj['haschildren'] = isset($parentPaths[$obj['path']]);
        }
        unset($obj);
        return $objects;
    }

    /**
     * Return a flat array of all ZFS snapshots.
     */
    private static function getAllSnapshotsData(): array
    {
        $prefix  = 'root/pool-';
        $objects = [];
        $cmd     = 'zfs list -H -p -t snapshot -o name,used,refer,encryption 2>&1';
        self::zfsExec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 4) {
                continue;
            }
            [$path, $used, $refer, $enc] = $parts;
            if (!preg_match('/(.*)\@(.*)$/', $path, $result) || count($result) < 3) {
                continue;
            }
            $objects[] = [
                'id'         => $prefix . $path,
                'parent'     => $result[1],
                'name'       => $result[2],
                'type'       => 'Snapshot',
                'icon'       => 'images/zfs_snap.png',
                'path'       => $path,
                'used'       => (int)$used,
                'refer'      => (int)$refer,
                'encryption' => strtoupper(trim($enc)),
            ];
        }
        return $objects;
    }

    /**
     * Fetch physical size, allocated, and free bytes for all pools via
     * zpool list -Hp. Returns an array keyed by pool name.
     */
    private static function zpoolFetchAllSizes(): array
    {
        $result = [];
        if (!self::zfsTryExec('zpool list -Hp -o name,size,alloc,free,frag,dedup,cap 2>&1', $out)) {
            return $result;
        }
        foreach ($out as $line) {
            $parts = preg_split('/\t/', trim($line));
            if (count($parts) < 7) {
                continue;
            }
            [$name, $size, $alloc, $free, $frag, $dedup, $cap] = $parts;
            $result[$name] = [
                'size'     => ($size  !== '-') ? (int)$size  : 0,
                'alloc'    => ($alloc !== '-') ? (int)$alloc : 0,
                'free'     => ($free  !== '-') ? (int)$free  : 0,
                'frag'     => ($frag  !== '-') ? (float)$frag : 0,
                'dedup'    => ($dedup !== '-') ? trim($dedup) : '1.00x',
                'capacity' => ($cap   !== '-') ? (int)$cap   : 0,
            ];
        }
        return $result;
    }

    /**
     * Fetch health, scrub, and vdev-removal status for all pools.
     * Uses a single `zpool status -j` for structured data plus one
     * `zpool status` text parse to pick up the remove: section which
     * is not included in the JSON output.
     * Returns an array keyed by pool name with keys: state, status,
     * scrubstate, scrubactive, lastscrub, removalstate, removalactive.
     */
    private static function zpoolFetchAllStatus(): array
    {
        $result = [];
        if (!self::zfsTryExec('zpool status -j 2>&1', $out)) {
            return $result;
        }
        $json = json_decode(implode('', $out), true);
        if (!is_array($json) || !isset($json['pools'])) {
            return $result;
        }
        foreach ($json['pools'] as $name => $pool) {
            $state      = $pool['state'] ?? 'UNKNOWN';
            $errorCount = (int)($pool['error_count'] ?? 0);
            $status     = ($errorCount === 0) ? 'OK' : 'Error';
            $scan       = $pool['scan_stats'] ?? [];
            $scanState  = strtoupper($scan['state'] ?? 'NONE');
            $scrubPause = $scan['scrub_pause'] ?? '-';
            switch ($scanState) {
                case 'SCANNING': $scrubstate = ($scrubPause !== '-') ? 'paused' : 'running'; break;
                case 'CANCELED': $scrubstate = 'stopped';   break;
                case 'FINISHED': $scrubstate = 'completed'; break;
                default:         $scrubstate = 'none';      break;
            }
            switch ($scrubstate) {
                case 'running':
                    $lastscrub = 'in progress since ' . ($scan['start_time'] ?? 'Unknown');
                    break;
                case 'completed':
                    $lastscrub = $scan['end_time'] ?? 'Unknown';
                    break;
                case 'paused':
                    $lastscrub = 'paused (started ' . ($scan['start_time'] ?? 'Unknown') . ')';
                    break;
                case 'stopped':
                    $lastscrub = 'canceled on ' . ($scan['end_time'] ?? 'Unknown');
                    break;
                default:
                    $lastscrub = 'Never';
                    break;
            }
            $result[$name] = [
                'state'         => $state,
                'status'        => $status,
                'scrubstate'    => $scrubstate,
                'scrubactive'   => in_array($scrubstate, ['running', 'paused']),
                'lastscrub'     => $lastscrub,
                'removalstate'  => 'none',
                'removalactive' => false,
            ];
        }
        // The JSON output does not include removal state, so parse the text
        // output of `zpool status` to find the remove: section per pool.
        if (self::zfsTryExec('zpool status 2>&1', $textOut)) {
            $currentPool  = '';
            $inRemove     = false;
            $removeLine   = '';
            foreach ($textOut as $line) {
                if (preg_match('/^\s*pool:\s*(\S+)/', $line, $m)) {
                    // Flush previous pool's removal state before moving on.
                    if ($currentPool !== '' && isset($result[$currentPool])) {
                        $result[$currentPool] = array_merge(
                            $result[$currentPool],
                            self::parseRemovalLine($removeLine)
                        );
                    }
                    $currentPool = $m[1];
                    $inRemove    = false;
                    $removeLine  = '';
                    continue;
                }
                if (preg_match('/^\s*remove\s*:\s*(.*)/', $line, $m)) {
                    $inRemove   = true;
                    $removeLine = trim($m[1]);
                    continue;
                }
                // A new labelled section ends the remove block.
                if ($inRemove && preg_match('/^\s*\w[\w ]*\s*:/', $line)) {
                    $inRemove = false;
                }
                if ($inRemove && $removeLine !== '') {
                    $removeLine .= ' ' . trim($line);
                }
            }
            // Flush the last pool.
            if ($currentPool !== '' && isset($result[$currentPool])) {
                $result[$currentPool] = array_merge(
                    $result[$currentPool],
                    self::parseRemovalLine($removeLine)
                );
            }
        }
        return $result;
    }

    /**
     * Classify a `remove:` section text into removalstate / removalactive.
     */
    private static function parseRemovalLine(string $line): array
    {
        $line = trim($line);
        if ($line === '') {
            return ['removalstate' => 'none', 'removalactive' => false];
        }
        if (stripos($line, 'Evacuating') !== false
            || stripos($line, 'in progress') !== false) {
            return ['removalstate' => 'active', 'removalactive' => true];
        }
        if (stripos($line, 'Completed') !== false) {
            return ['removalstate' => 'completed', 'removalactive' => false];
        }
        if (stripos($line, 'canceled') !== false) {
            return ['removalstate' => 'canceled', 'removalactive' => false];
        }
        // remove: section exists but doesn't match known patterns.
        return ['removalstate' => 'active', 'removalactive' => true];
    }

    /**
     * Return a set of ZFS dataset fsnames that have at least one shared folder
     * referencing them, keyed by fsname for O(1) lookup.
     */
    private static function buildSharedFsnames(): array
    {
        $db = \OMV\Config\Database::getInstance();
        try {
            $sharedFolders = $db->getAssoc('conf.system.sharedfolder');
        } catch (\Exception $e) {
            return [];
        }
        if (empty($sharedFolders)) {
            return [];
        }
        // getAssoc returns a single assoc when there is only one item.
        if (isset($sharedFolders['mntentref'])) {
            $sharedFolders = [$sharedFolders];
        }
        $usedRefs = [];
        foreach ($sharedFolders as $sf) {
            $usedRefs[$sf['mntentref']] = true;
        }
        try {
            $mntents = $db->getAssoc('conf.system.filesystem.mountpoint');
        } catch (\Exception $e) {
            return [];
        }
        if (empty($mntents)) {
            return [];
        }
        if (isset($mntents['fsname'])) {
            $mntents = [$mntents];
        }
        $result = [];
        foreach ($mntents as $mntent) {
            if (isset($usedRefs[$mntent['uuid']])) {
                $result[$mntent['fsname']] = true;
            }
        }
        return $result;
    }

    /**
     * Return true if the given pool or dataset has any child datasets, volumes, or snapshots.
     */
    private static function hasChildren(string $name): bool
    {
        self::zfsTryExec(
            sprintf('zfs list -H -d 1 -t filesystem,volume %s 2>&1', escapeshellarg($name)),
            $out
        );
        // First line is the object itself; any additional lines are child datasets.
        return count($out) > 1;
    }

    /**
     * Return datasets that are clones of any snapshot owned by $name.
     * Used to give a specific error when deleting a promoted clone whose
     * snapshot still has dependents (e.g. the original dataset after promote).
     *
     * @return string[]  Clone dataset names, or empty array if none.
     */
    private static function getSnapshotDependentClones(string $name): array
    {
        $clones = [];
        if (!self::zfsTryExec(
            sprintf('zfs list -H -d 1 -t snapshot -o clones %s 2>/dev/null', escapeshellarg($name)),
            $out
        )) {
            return [];
        }
        foreach ($out as $line) {
            $line = trim($line);
            if ($line === '' || $line === '-') {
                continue;
            }
            foreach (explode(',', $line) as $clone) {
                $clone = trim($clone);
                if ($clone !== '') {
                    $clones[] = $clone;
                }
            }
        }
        return $clones;
    }

    /**
     * Return the path where a dataset's local auto-unlock keyfile is stored.
     */
    private static function getKeyfilePath(string $name): string
    {
        $safe = str_replace('/', '_', $name);
        return '/etc/zfs/keys/' . $safe . '.key';
    }

    /**
     * Return the path where a dataset's remote auto-unlock metadata is stored.
     */
    private static function getRemoteKeyMetaPath(string $name): string
    {
        $safe = str_replace('/', '_', $name);
        return '/etc/zfs/remote-keys/' . $safe . '.json';
    }

    /**
     * Enable/disable the zfs-remote-unlock systemd service and its ordering
     * drop-in depending on whether any remote-key metadata files currently exist.
     *
     * The drop-in at /etc/systemd/system/zfs-load-key@.service.d/wait-for-remote-unlock.conf
     * is managed dynamically so that zfs-load-key@ instances only pull in
     * zfs-remote-unlock.service when remote unlock is actually configured.
     * Without this, the drop-in would cause every zfs-load-key@ instance to
     * Wants/After zfs-remote-unlock.service even after remote unlock is disabled,
     * resulting in the service being invoked on every boot.
     */
    private static function manageRemoteUnlockService(): void
    {
        $metaDir    = '/etc/zfs/remote-keys';
        $hasRemote  = is_dir($metaDir) && !empty(glob($metaDir . '/*.json'));
        $dropinDir  = '/etc/systemd/system/zfs-load-key@.service.d';
        $dropinFile = $dropinDir . '/wait-for-remote-unlock.conf';
        if ($hasRemote) {
            // Ensure the ordering drop-in exists so zfs-load-key@ instances
            // wait for zfs-remote-unlock.service to load remote keys first.
            // Use shell commands rather than PHP file functions: open_basedir
            // restrictions in the OMV engined PHP context block direct access
            // to /etc/systemd/system/, but subprocess commands are not affected.
            self::zfsTryExec(sprintf('mkdir -p %s 2>/dev/null', escapeshellarg($dropinDir)));
            self::zfsTryExec(sprintf(
                'test -f %s || printf "%%s\n%%s\n%%s\n" "[Unit]" '
                . '"After=zfs-remote-unlock.service" '
                . '"Wants=zfs-remote-unlock.service" > %s 2>/dev/null',
                escapeshellarg($dropinFile),
                escapeshellarg($dropinFile)));
            self::zfsTryExec('systemctl enable --now zfs-remote-unlock.service 2>&1');
        } else {
            // Remove the ordering drop-in so zfs-load-key@ instances no longer
            // pull in zfs-remote-unlock.service when remote unlock is not configured.
            self::zfsTryExec(sprintf('rm -f %s 2>/dev/null', escapeshellarg($dropinFile)));
            self::zfsTryExec('systemctl disable --now zfs-remote-unlock.service 2>&1');
        }
    }

    /**
     * Update the ZFS list cache for a pool so that zfs-mount-generator can
     * generate the appropriate systemd key-load units at boot.
     * The cache file at /etc/zfs/zfs-list.cache/<pool> must exist and be
     * populated for encrypted datasets with keylocation=file:// to
     * auto-unlock during boot.
     */
    private static function updateZfsListCache(string $poolName): void
    {
        $cacheDir  = '/etc/zfs/zfs-list.cache';
        $cacheFile = $cacheDir . '/' . $poolName;
        if (!is_dir($cacheDir)) {
            mkdir($cacheDir, 0755, true);
        }
        $props = 'name,mountpoint,canmount,atime,relatime,devices,exec,'
               . 'readonly,setuid,nbmand,encroot,keylocation,'
               . 'org.openzfs.systemd:requires,'
               . 'org.openzfs.systemd:requires-mounts-for,'
               . 'org.openzfs.systemd:before,'
               . 'org.openzfs.systemd:after,'
               . 'org.openzfs.systemd:wanted-by,'
               . 'org.openzfs.systemd:required-by,'
               . 'org.openzfs.systemd:nofail,'
               . 'org.openzfs.systemd:ignore';
        $cmd = sprintf('zfs list -H -t filesystem -o %s -r %s 2>/dev/null',
            escapeshellarg($props),
            escapeshellarg($poolName));
        self::zfsTryExec($cmd, $lines);
        sort($lines);
        file_put_contents($cacheFile, implode("\n", $lines) . "\n");
    }

    /**
     * Return encryption property values for a dataset.
     */
    private static function zfsGetEncryptionStatus(string $name): array
    {
        $cmd = sprintf('zfs get -H -p encryption,encryptionroot,keystatus,keyformat,keylocation %s 2>&1',
            escapeshellarg($name));
        self::zfsExec($cmd, $out, $res);
        $status = [];
        foreach ($out as $line) {
            if (empty($line)) {
                continue;
            }
            $parts = preg_split('/\t+/', $line);
            if (count($parts) >= 3) {
                $status[$parts[1]] = ['value' => $parts[2], 'source' => $parts[3] ?? '-'];
            }
        }
        return $status;
    }

    /**
     * Flatten nested encryption status into a flat array matching the form fields.
     */
    private static function flattenEncryptionStatus(string $name, array $status): array
    {
        $keylocation = $status['keylocation']['value'] ?? 'prompt';
        $autounlock  = (strpos($keylocation, 'file://') === 0 && file_exists(self::getKeyfilePath($name)))
                    || file_exists(self::getRemoteKeyMetaPath($name));
        return [
            'name'           => $name,
            'encryption'     => isset($status['encryption']['value'])
                ? strtoupper($status['encryption']['value']) : 'OFF',
            'encryptionroot' => $status['encryptionroot']['value'] ?? '-',
            'keystatus'      => $status['keystatus']['value'] ?? '-',
            'keyformat'      => $status['keyformat']['value'] ?? '-',
            'keylocation'    => $keylocation,
            'autounlock'     => $autounlock,
        ];
    }
}
