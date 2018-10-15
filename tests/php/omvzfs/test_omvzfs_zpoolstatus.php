#!/usr/bin/phpunit -c/etc/openmediavault
<?php

require_once 'PHPUnit/Autoload.php';

// Relative "base" path used
$filePath = dirname(__FILE__);

require_once $filePath . "/../../../usr/share/omvzfs/ZpoolStatus.php";

require_once $filePath . "/mocks/test_omvzfs_zpoolstatus/cmdoutput_mocks.php";

class test_omvzfs_zpoolstatus extends \PHPUnit\Framework\TestCase {
    /**
     * @dataProvider parseStatusDataProvider
     */
    public function testParseStatus($cmdOutput, $expectedStructure) {
        $result = OMVModuleZFSZpoolStatus::parseStatus($cmdOutput);

        $this->assertEquals($expectedStructure, $result);
    }

    /**
     * @dataProvider vdevsGetterDataProvider
     */
    public function testVDevGetter($cmdOutput, $expectedVDevs) {
        $zpoolStatus = new OMVModuleZFSZpoolStatus($cmdOutput);

        $vdevs = $zpoolStatus->getVDevs();

        $this->assertCount(count($expectedVDevs), $vdevs);

        foreach ($expectedVDevs as $vdevIdx => $expectedVDev) {
            $this->assertEquals(
                $expectedVDev["poolName"],
                $vdevs[$vdevIdx]->getPool()
            );
            $this->assertEquals(
                $expectedVDev["type"],
                $vdevs[$vdevIdx]->getType()
            );
            $this->assertEquals(
                $expectedVDev["devices"],
                $vdevs[$vdevIdx]->getDisks()
            );
        }
    }

    public function parseStatusDataProvider() {
        return [
            "simple, healthy pool's output" => [
                Test\OMVZFS\ZpoolStatus\Mocks\SimpleMock::getCmdOutput(),
                Test\OMVZFS\ZpoolStatus\Mocks\SimpleMock::getExpectedParsedStructure()
            ],
            "complex pool's output (vdevs, logs & spares)" => [
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getCmdOutput(),
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getExpectedParsedStructure()
            ]
        ];
    }

    public function vdevsGetterDataProvider() {
        return [
            "simple, healthy pool's output" => [
                Test\OMVZFS\ZpoolStatus\Mocks\SimpleMock::getCmdOutput(),
                [
                    [
                        "poolName" => "testpool",
                        "type" => OMVModuleZFSVdevType::OMVMODULEZFSMIRROR,
                        "devices" => [
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                        ]
                    ]
                ]
            ],
            "complex pool's output (vdevs, logs & spares)" => [
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getCmdOutput(),
                [
                    [
                        "poolName" => "testpool",
                        "type" => OMVModuleZFSVdevType::OMVMODULEZFSMIRROR,
                        "devices" => [
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                        ]
                    ],
                    [
                        "poolName" => "testpool",
                        "type" => OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1,
                        "devices" => [
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2",
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2",
                            "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2"
                        ]
                    ]
                ]
            ]
        ];
    }
}
