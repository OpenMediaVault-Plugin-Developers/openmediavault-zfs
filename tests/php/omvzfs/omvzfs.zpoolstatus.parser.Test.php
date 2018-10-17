<?php

require_once 'PHPUnit/Autoload.php';

// Relative "base" path used
$filePath = dirname(__FILE__);

require_once $filePath . "/../../../usr/share/omvzfs/ZpoolStatus.php";

class test_omvzfs_zpoolstatus_parser extends \PHPUnit\Framework\TestCase {
    /**
     * @dataProvider parseStatusDataProvider
     */
    public function testParseStatus($cmdOutput, $expectedStructure) {
        $result = OMVModuleZFSZpoolStatus::parseStatus($cmdOutput);

        $this->assertEquals($expectedStructure, $result);
    }

    public function parseStatusDataProvider() {
        $basepath = dirname(__FILE__);

        $mocks = [
            "cmd_output.zpool_status.simplepool_sata_bydev",
            "cmd_output.zpool_status.simplepool_sata_byid",
            "cmd_output.zpool_status.simplepool_nvme_bydev",
            "cmd_output.zpool_status.simplepool_nvme_byid",
            "cmd_output.zpool_status.simplepool_mixed_bydev",
            "cmd_output.zpool_status.simplepool_mixed_byid",
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
        ];

        $datasets = [];

        foreach ($mocks as $mockName) {
            $datasets[$mockName] = [
                // mock file content
                explode(
                    "\n",
                    file_get_contents(
                        $basepath . "/mocks/test_omvzfs_zpoolstatus_parser/" . $mockName . ".txt"
                    )
                ),
                // mock expected structure
                json_decode(
                    file_get_contents(
                        $basepath . "/expectations/test_omvzfs_zpoolstatus_parser/parsed_structure/" . $mockName . ".json"
                    ),
                    true
                )
            ];
        }

        return $datasets;
    }
}
