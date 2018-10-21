<?php

// Relative "base" path used
$filePath = dirname(__FILE__);

require_once $filePath . "/utils.php";
require_once $filePath . "/../../../usr/share/omvzfs/ZpoolStatus.php";

class OMVZFSZpoolStatusParserTest extends \PHPUnit\Framework\TestCase {
    /**
     * @dataProvider parseStatusDataProvider
     */
    public function testParseStatus($cmdOutput, $expectedStructure) {
        $result = OMVModuleZFSZpoolStatus::parseStatus($cmdOutput);

        $this->assertEquals($expectedStructure, $result);
    }

    public function parseStatusDataProvider() {
        $mocks = [
            "cmd_output.zpool_status.simplepool_sata_bydev",
            "cmd_output.zpool_status.simplepool_sata_byid",
            "cmd_output.zpool_status.simplepool_nvme_bydev",
            "cmd_output.zpool_status.simplepool_nvme_byid",
            "cmd_output.zpool_status.simplepool_devicemapper_bydev",
            "cmd_output.zpool_status.simplepool_devicemapper_byid",
            "cmd_output.zpool_status.simplepool_mixed_bydev",
            "cmd_output.zpool_status.simplepool_mixed_byid",
            "cmd_output.zpool_status.simplepool_mixed_refsmixed",
            "cmd_output.zpool_status.redundantpool_mirror_single",
            "cmd_output.zpool_status.redundantpool_mirror_double",
            "cmd_output.zpool_status.redundantpool_raidz1_single",
            "cmd_output.zpool_status.redundantpool_raidz1_extended",
            "cmd_output.zpool_status.redundantpool_raidz1_double",
            "cmd_output.zpool_status.redundantpool_raidz2_single",
            "cmd_output.zpool_status.redundantpool_raidz2_extended",
            "cmd_output.zpool_status.redundantpool_raidz2_double",
            "cmd_output.zpool_status.redundantpool_raidz3_single",
            "cmd_output.zpool_status.redundantpool_raidz3_extended",
            "cmd_output.zpool_status.redundantpool_raidz3_double",
            "cmd_output.zpool_status.redundantpool_mixed",
            "cmd_output.zpool_status.poolwithcache_simplecache",
            "cmd_output.zpool_status.poolwithcache_doublecache",
            "cmd_output.zpool_status.poolwithlogs_simplelog",
            "cmd_output.zpool_status.poolwithlogs_doublelog",
            "cmd_output.zpool_status.poolwithlogs_mirrorlog",
            "cmd_output.zpool_status.poolwithlogs_doublemirrorlog",
            "cmd_output.zpool_status.poolwithspares_singlespare",
            "cmd_output.zpool_status.poolwithspares_multiplespares",
            "cmd_output.zpool_status.complexpool_createdupfront",
            "cmd_output.zpool_status.complexpool_outoforder",
            "cmd_output.zpool_status.degradedpool_simplemirror",
            "cmd_output.zpool_status.degradedpool_multidegradation",
            "cmd_output.zpool_status.poolerror_zfs_8000_9P",
        ];

        $datasets = [];

        foreach ($mocks as $mockName) {
            $datasets[$mockName] = [
                // mock file content
                OMVZFSTestUtils::loadCmdOutputFile(
                    "/mocks/omvzfs.zpoolstatus/" . $mockName . ".txt"
                ),
                // mock expected structure
                OMVZFSTestUtils::loadJSONFile(
                    "/expectations/omvzfs.zpoolstatus/parsed_structure/" . $mockName . ".json"
                )
            ];
        }

        return $datasets;
    }
}
