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
        $cmd = sprintf('/usr/sbin/omv-zfs-snapshot %s %s %d 2>&1',
            escapeshellarg($j['dataset']),
            escapeshellarg($j['prefix']),
            (int)$j['retention']);
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
        // Add encryption options if requested.
        $encrypt = !empty($params['encrypt']);
        if ($encrypt) {
            $enctype = $params['encryptiontype'] ?? 'aes-256-gcm';
            $opts .= sprintf('-O encryption=%s -O keyformat=passphrase -O keylocation=prompt ',
                escapeshellarg($enctype));
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
        // Create the pool, piping the passphrase via stdin if encryption is enabled.
        $cmd = sprintf('zpool create %s%s %s 2>&1',
            $opts,
            escapeshellarg($params['name']),
            self::buildVdevString($params['pooltype'], $disks));
        if ($encrypt) {
            self::execWithStdin($cmd, $params['key']);
        } else {
            self::zfsExec($cmd, $out, $res);
        }
        // Set compression if requested.
        if ($params['compress']) {
            $cmd = sprintf('zfs set compression=%s %s 2>&1',
                escapeshellarg($params['compresstype']),
                escapeshellarg($params['name']));
            self::zfsExec($cmd, $out, $res);
        }
        // Enable auto-unlock if requested: save keyfile and update keylocation.
        if ($encrypt && !empty($params['autounlock'])) {
            $keyfile = self::getKeyfilePath($params['name']);
            $keydir  = dirname($keyfile);
            if (!is_dir($keydir)) {
                mkdir($keydir, 0700, true);
            }
            file_put_contents($keyfile, $params['key']);
            chmod($keyfile, 0600);
            self::zfsExec(sprintf('zfs set keylocation=%s %s 2>&1',
                escapeshellarg('file://' . $keyfile),
                escapeshellarg($params['name'])), $out, $res);
            self::updateZfsListCache($params['name']);
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
        // For log devices, prevent a dangerous stripe. If the pool already has a
        // log device, use 'zpool attach' to form or extend a mirror instead.
        if ($params['vdevtype'] === 'log') {
            $existing = self::findFirstLogDevice($params['pool']);
            if ($existing !== null) {
                $cmd = sprintf('zpool attach %s %s %s 2>&1',
                    escapeshellarg($params['pool']),
                    escapeshellarg($existing),
                    escapeshellarg($device));
                self::zfsExec($cmd, $out, $res);
                return;
            }
        }
        $cmd = sprintf('zpool add %s %s %s 2>&1',
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
                break;
            case 'snapshot':
                $name = $params['path'] . '@' . $params['name'];
                $cmd = sprintf('zfs snapshot %s 2>&1', escapeshellarg($name));
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
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $snapshotName = $params['name'] . '@' . date('Y-m-d_H-i-s');
        $cmd = sprintf('zfs snapshot %s 2>&1', escapeshellarg($snapshotName));
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
        self::zfsExec($cmd, $out, $res);
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
                $cmd = sprintf('zfs destroy %s 2>&1', escapeshellarg($name));
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
    }

    public function deleteObjectBg($params, $context)
    {
        return $this->callMethodBg('deleteObject', $params, $context);
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
            } else {
                $cmd = sprintf('zfs set %s=%s %s 2>&1',
                    escapeshellarg($property['property']),
                    escapeshellarg($property['value']),
                    escapeshellarg($params['name']));
                if ($property['property'] === 'mountpoint') {
                    self::relocateOMVMntEnt($context, $params['name']);
                    \OMV\Engine\Notify\Dispatcher::getInstance()->notify(OMV_NOTIFY_MODIFY,
                        'org.openmediavault.conf.system.sharedfolder', $context);
                }
            }
            self::zfsExec($cmd, $out, $res);
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
        $cmd = sprintf('zpool status -P %s 2>&1', escapeshellarg($params['name']));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $devices     = [];
        $inConfig    = false;
        $currentVdev = '';
        $vdevDepth   = -1;
        foreach ($out as $line) {
            if (preg_match('/^\s*NAME\s+STATE/', $line)) {
                $inConfig = true;
                continue;
            }
            if (!$inConfig) continue;
            if (trim($line) === '') break;
            $trimmed = ltrim($line);
            $depth   = strlen($line) - strlen($trimmed);
            // Vdev group line (mirror-N, raidz*-N, replacing-N, spare)
            if (preg_match('/^(mirror-\d+|raidz\d*-\d+|replacing-\d+|spare)\s/', $trimmed, $vm)) {
                $currentVdev = $vm[1];
                $vdevDepth   = $depth;
                continue;
            }
            // Device line (absolute path)
            if (preg_match('/^(\/\S+)\s+(\S+)/', $trimmed, $m)) {
                // A device at the same or lesser indent than the vdev line is a
                // stripe member — it does not belong to that vdev.
                if ($vdevDepth >= 0 && $depth <= $vdevDepth) {
                    $currentVdev = '';
                    $vdevDepth   = -1;
                }
                $vdev  = $currentVdev;
                $label = $vdev
                    ? $m[1] . '  [' . $vdev . '] [' . $m[2] . ']'
                    : $m[1] . '  [stripe] [' . $m[2] . ']';
                $devices[] = [
                    'devicefile' => $m[1],
                    'state'      => $m[2],
                    'vdev'       => $vdev,
                    'label'      => $label,
                ];
            }
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

        // Check whether raidz_expansion is active for this pool.
        $expansionActive = false;
        $featCmd = sprintf('zpool get -H -o value feature@raidz_expansion %s 2>/dev/null',
            escapeshellarg($pool));
        if (self::zfsTryExec($featCmd, $featOut)) {
            $expansionActive = (trim($featOut[0] ?? '') === 'active');
        }

        $cmd = sprintf('zpool status -P %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $anchors        = [];
        $inConfig       = false;
        $currentVdev    = '';
        $vdevDepth      = -1;
        $raidzVdevSeen  = [];
        $inClassSection = false;
        // Class headers that introduce non-data vdev sections (logs, cache, spares, etc.).
        $classSectionNames = ['logs' => true, 'cache' => true, 'spares' => true,
                              'special' => true, 'dedup' => true];
        foreach ($out as $line) {
            if (preg_match('/^\s*NAME\s+STATE/', $line)) {
                $inConfig = true;
                continue;
            }
            if (!$inConfig) continue;
            if (trim($line) === '') break;
            $trimmed = ltrim($line);
            $depth   = strlen($line) - strlen($trimmed);
            // Detect class header lines (logs, cache, spares, special, dedup).
            // These appear at the same indent level as data vdevs and introduce
            // special-device sections whose members must not be offered as attach
            // anchors (zpool attach is only valid for data vdevs / mirrors).
            $token0 = preg_split('/\s+/', $trimmed)[0];
            if (isset($classSectionNames[$token0])) {
                $inClassSection = true;
                $currentVdev    = '';
                $vdevDepth      = -1;
                continue;
            }
            if ($inClassSection) continue;
            // Vdev group line (mirror-N, raidzN-N, replacing-N, spare).
            if (preg_match('/^(mirror-\d+|raidz\d*-\d+|replacing-\d+|spare)\s/', $trimmed, $vm)) {
                $currentVdev = $vm[1];
                $vdevDepth   = $depth;
                // For RAIDZ vdevs with expansion enabled, add the vdev name once.
                if ($expansionActive && preg_match('/^raidz\d*-\d+$/', $vm[1])
                    && !isset($raidzVdevSeen[$vm[1]])) {
                    $raidzVdevSeen[$vm[1]] = true;
                    $anchors[] = [
                        'devicefile' => $vm[1],
                        'state'      => '',
                        'vdev'       => $vm[1],
                        'label'      => $vm[1] . '  [RAIDZ expansion]',
                    ];
                }
                continue;
            }
            // Device line (absolute path).
            if (preg_match('/^(\/\S+)\s+(\S+)/', $trimmed, $m)) {
                if ($vdevDepth >= 0 && $depth <= $vdevDepth) {
                    $currentVdev = '';
                    $vdevDepth   = -1;
                }
                $vdev = $currentVdev;
                // Never offer RAIDZ member devices as anchors — zpool attach is
                // invalid on RAIDZ members.  When expansion is active the RAIDZ
                // vdev name entry is already in the list above.
                if (preg_match('/^raidz/', $vdev)) {
                    continue;
                }
                $label = $vdev
                    ? $m[1] . '  [' . $vdev . '] [' . $m[2] . ']'
                    : $m[1] . '  [stripe] [' . $m[2] . ']';
                $anchors[] = [
                    'devicefile' => $m[1],
                    'state'      => $m[2],
                    'vdev'       => $vdev,
                    'label'      => $label,
                ];
            }
        }
        return $anchors;
    }

    public function getTopLevelVdevs($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.zfs.name');
        $pool = $params['name'];
        $cmd  = sprintf('zpool status -P %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return [];
        }
        $vdevs      = [];
        $inConfig   = false;
        $poolDepth  = -1;
        $topDepth   = -1;
        $classType  = '';
        $classDepth = -1;
        $classMap   = ['cache' => 'cache', 'logs' => 'log', 'spares' => 'spare',
                       'special' => 'special', 'dedup' => 'dedup'];
        foreach ($out as $line) {
            if (preg_match('/^\s*NAME\s+STATE/', $line)) {
                $inConfig = true;
                continue;
            }
            if (!$inConfig) continue;
            if (trim($line) === '') break;
            $trimmed = ltrim($line);
            $depth   = strlen($line) - strlen($trimmed);
            $parts   = preg_split('/\s+/', trim($trimmed));
            $token   = $parts[0];
            $state   = $parts[1] ?? '';
            // First line in the config section is the pool name line itself.
            if ($poolDepth === -1) {
                $poolDepth = $depth;
                continue;
            }
            // First line after the pool establishes the top-level indent.
            if ($topDepth === -1) {
                $topDepth = $depth;
            }
            if ($depth === $topDepth) {
                // Reset class tracking whenever we're back at the top level.
                $classType  = '';
                $classDepth = -1;
                // Class header lines have no state column.
                if (isset($classMap[$token])) {
                    $classType  = $classMap[$token];
                    $classDepth = $depth;
                    continue;
                }
                // Data vdev or stripe device.
                $type      = self::vdevTypeFromToken($token);
                $removable = !str_starts_with($type, 'raidz');
                if ($removable) {
                    $vdevs[] = [
                        'vdev'  => $token,
                        'type'  => $type,
                        'state' => $state,
                        'label' => 'Data: ' . $token . '  [' . $type . '] [' . $state . ']',
                    ];
                }
            } elseif ($classDepth >= 0 && $depth > $classDepth
                      && str_starts_with($token, '/')) {
                // Individual device under a cache / log / spare header.
                $prefix = ucfirst($classType) . ': ';
                $vdevs[] = [
                    'vdev'  => $token,
                    'type'  => $classType,
                    'state' => $state,
                    'label' => $prefix . $token . '  [' . $state . ']',
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
        $cmd = 'zfs load-key ' . escapeshellarg($params['name']);
        self::execWithStdin($cmd, $params['key']);
        // Mount the dataset now that the key is loaded.
        self::zfsTryExec(sprintf('zfs mount %s 2>&1', escapeshellarg($params['name'])));
        self::addOMVMntEntForDataset($context, $params['name']);
        self::dirtyModules(['collectd', 'fstab', 'monit', 'quota']);
        return ['status' => 'loaded'];
    }

    public function unloadEncryptionKey($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, 'rpc.zfs.unloadencryptionkey');
        // Collect mountpoints before unmounting so we can lazy-unmount any that
        // remain busy.  zfs unmount -f uses MNT_FORCE which can still fail when a
        // process holds an open file handle; umount -l (MNT_DETACH) always detaches
        // the filesystem from the VFS immediately so zfs unload-key can proceed.
        self::zfsTryExec(sprintf(
            'zfs list -H -r -t filesystem -o mountpoint %s 2>/dev/null',
            escapeshellarg($params['name'])
        ), $mountpoints);
        self::zfsTryExec(sprintf('zfs unmount -f -r %s 2>&1', escapeshellarg($params['name'])));
        foreach (($mountpoints ?? []) as $mp) {
            $mp = trim($mp);
            if ($mp === '' || in_array($mp, ['none', 'legacy', '-'])) {
                continue;
            }
            self::zfsTryExec(sprintf('umount -l %s 2>/dev/null', escapeshellarg($mp)));
        }
        $cmd = sprintf('zfs unload-key -r %s 2>&1', escapeshellarg($params['name']));
        self::zfsExec($cmd, $out, $res);
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
     * Return the first absolute device path found under the 'logs' section of
     * 'zpool status -P <pool>', or null if no log devices exist.
     * Works for both single log devices and log mirrors (returns one member).
     */
    private static function findFirstLogDevice(string $pool): ?string
    {
        $cmd = sprintf('zpool status -P %s 2>&1', escapeshellarg($pool));
        if (!self::zfsTryExec($cmd, $out)) {
            return null;
        }
        $inConfig  = false;
        $inLogs    = false;
        $poolDepth = -1;
        $logsDepth = -1;
        foreach ($out as $line) {
            if (preg_match('/^\s*NAME\s+STATE/', $line)) {
                $inConfig = true;
                continue;
            }
            if (!$inConfig) continue;
            if (trim($line) === '') break;
            $trimmed = ltrim($line);
            $depth   = strlen($line) - strlen($trimmed);
            $token   = preg_split('/\s+/', $trimmed)[0];
            if ($poolDepth === -1) { $poolDepth = $depth; continue; }
            // 'logs' appears at poolDepth (same level as the pool name),
            // not at the top-vdev level (where mirror-0, raidz1-0 etc. live).
            if ($depth === $poolDepth) {
                $inLogs    = ($token === 'logs');
                $logsDepth = $inLogs ? $depth : -1;
            } elseif ($inLogs && $depth > $logsDepth && str_starts_with($token, '/')) {
                return $token;
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
     * Get all physical device paths from a pool via zpool status -P,
     * excluding devices with states in $excludeStates (e.g. ['INUSE','UNAVAIL','SPLIT']).
     */
    public static function zpoolGetAllDevices(string $poolName, array $excludeStates = []): array
    {
        $cmd = sprintf('zpool status -P %s 2>&1', escapeshellarg($poolName));
        self::zfsExec($cmd, $out, $res);
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
            // Lines with absolute device paths start with / after leading whitespace.
            if (preg_match('/^\s+(\/\S+)\s+(\S+)/', $line, $m)) {
                if (!in_array($m[2], $excludeStates)) {
                    $devices[] = $m[1];
                }
            }
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
    private static function setMntentProperty(string $uuid, string $fsname): void
    {
        if (!self::isPoolImported(self::getPoolFromFsname($fsname))) {
            return;
        }
        $cmd = sprintf('zfs set omvzfsplugin:uuid=%s %s 2>&1',
            escapeshellarg($uuid), escapeshellarg($fsname));
        self::zfsTryExec($cmd);
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
        $object = \OMV\Rpc\Rpc::call('FsTab', 'getByDir', ['dir' => $mountpoint], $context);
        if (!$object || $object['type'] !== 'zfs' || $object['dir'] !== $mountpoint) {
            throw new \OMV\Exception(sprintf('No ZFS mntent found for: %s', $mountpoint));
        }
        \OMV\Rpc\Rpc::call('FsTab', 'delete', ['uuid' => $object['uuid']], $context);
        if (self::zfsFilesystemExists($name)) {
            self::zfsExec(sprintf('zfs destroy %s 2>&1', escapeshellarg($name)), $out, $res);
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
                $object['dir'] = $mountpoint;
                $config        = $db->get('conf.system.filesystem.mountpoint', $object['uuid']);
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
    private static function addOMVMntEntForDataset($context, string $fsname): void
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
        // Skip if an mntent entry already exists for this dataset.
        $entries = \OMV\Rpc\Rpc::call('FsTab', 'enumerateEntries', [], $context);
        foreach ($entries as $entry) {
            if (isset($entry['fsname']) && $entry['fsname'] === $fsname) {
                self::setMntentProperty($entry['uuid'], $fsname);
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
        self::setMntentProperty($uuid, $fsname);
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
            self::zfsExec('zfs list -H -o name,mountpoint', $out, $res);
            foreach ($out as $line) {
                $line = trim($line);
                if ($line === '') {
                    continue;
                }
                $parts = preg_split('/\s+/', $line, 2);
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
            // Read mountpoint and stored UUID from the ZFS dataset in one call.
            $cmd = sprintf('zfs get -H -p mountpoint,omvzfsplugin:uuid %s 2>&1',
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
            if ($oldUuid && $oldUuid !== $newUuid) {
                $matchingSfs = array_filter($sfObjects, function ($sf) use ($mountpoint) {
                    return isset($sf['mntent']['dir'])
                        && rtrim($sf['mntent']['dir'], '/') === rtrim($mountpoint, '/');
                });
                if (!empty($matchingSfs)) {
                    self::fixOMVSharedFolders($oldUuid, $newUuid, array_values($matchingSfs), $context);
                }
            }
            self::setMntentProperty($newUuid, $fsname);
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
            'zfs get -H -p encryption,keystatus,keylocation -t filesystem,volume 2>&1',
            $encOut
        );
        foreach ($encOut as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 3) {
                continue;
            }
            $encMap[$parts[0]][$parts[1]] = $parts[2];
        }
        // One zfs list call returns all the data we need for sizing/type/origin.
        $cmd = 'zfs list -H -p -t filesystem,volume '
             . '-o name,type,used,available,mountpoint,origin,volsize,logicalused,refreservation,quota,compression,compressratio,refer 2>&1';
        self::zfsExec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 13) {
                continue;
            }
            [$path, $type, $used, $available, $mountpoint,
             $origin, $volsize, $logicalused, $refreservation, $quota,
             $compression, $compressratio, $refer] = $parts;
            $isPool       = ($type === 'filesystem' && strpos($path, '/') === false);
            $isFilesystem = ($type === 'filesystem' && !$isPool);
            $isVolume     = ($type === 'volume');
            $enc          = strtoupper($encMap[$path]['encryption'] ?? 'off');
            $keystatus    = $encMap[$path]['keystatus']   ?? '-';
            $keylocation  = $encMap[$path]['keylocation'] ?? 'none';
            $encrypted    = ($enc !== 'OFF');
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
                    'encryption'    => $enc,
                    'keystatus'     => $keystatus,
                    'autounlock'    => $autounlock,
                    'hasshares'     => isset($sharedFsnames[$path]),
                    'compression'   => ($compression !== '-') ? $compression : 'off',
                    'compressratio' => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'         => ($refer !== '-') ? (int)$refer : 0,
                    'poolroot'      => true,
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
                    'encryption'    => $enc,
                    'keystatus'     => $keystatus,
                    'autounlock'    => $autounlock,
                    'hasshares'     => isset($sharedFsnames[$path]),
                    'compression'   => ($compression !== '-') ? $compression : 'off',
                    'compressratio' => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'         => ($refer !== '-') ? (int)$refer : 0,
                    'poolroot'      => false,
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
                    'encryption'    => $enc,
                    'keystatus'     => $keystatus,
                    'autounlock'    => $autounlock,
                    'hasshares'     => false,
                    'compression'   => ($compression !== '-') ? $compression : 'off',
                    'compressratio' => ($compressratio !== '-') ? trim($compressratio) : '1.00x',
                    'refer'         => ($refer !== '-') ? (int)$refer : 0,
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
        $cmd     = 'zfs list -H -p -t snapshot -o name,used,refer 2>&1';
        self::zfsExec($cmd, $out, $res);
        foreach ($out as $line) {
            $parts = preg_split('/\t/', $line);
            if (count($parts) < 3) {
                continue;
            }
            [$path, $used, $refer] = $parts;
            if (!preg_match('/(.*)\@(.*)$/', $path, $result) || count($result) < 3) {
                continue;
            }
            $objects[] = [
                'id'     => $prefix . $path,
                'parent' => $result[1],
                'name'   => $result[2],
                'type'   => 'Snapshot',
                'icon'   => 'images/zfs_snap.png',
                'path'   => $path,
                'used'   => (int)$used,
                'refer'  => (int)$refer,
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
            sprintf('zfs list -H -d 1 -t filesystem,volume,snapshot %s 2>&1', escapeshellarg($name)),
            $out
        );
        // First line is the object itself; any additional lines are children.
        return count($out) > 1;
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
     * Enable/disable the zfs-remote-unlock systemd service depending on
     * whether any remote-key metadata files currently exist.
     */
    private static function manageRemoteUnlockService(): void
    {
        $metaDir   = '/etc/zfs/remote-keys';
        $hasRemote = is_dir($metaDir) && !empty(glob($metaDir . '/*.json'));
        self::zfsTryExec('systemctl ' . ($hasRemote ? 'enable' : 'disable')
            . ' --now zfs-remote-unlock.service 2>&1');
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
