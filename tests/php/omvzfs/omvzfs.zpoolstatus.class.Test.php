<?php

require_once 'PHPUnit/Autoload.php';

// Relative "base" path used
$filePath = dirname(__FILE__);

require_once $filePath . "/../../../usr/share/omvzfs/ZpoolStatus.php";
require_once $filePath . "/../../../usr/share/omvzfs/VdevState.php";

require_once $filePath . "/mocks/test_omvzfs_zpoolstatus/cmdoutput_mocks.php";

class test_omvzfs_zpoolstatus extends \PHPUnit\Framework\TestCase {
    /**
     * @dataProvider allDevicesGetterDataProvider
     */
    public function testAllDevicesGetter($cmdOutput, $expectedDevices) {
        $zpoolStatus = new OMVModuleZFSZpoolStatus($cmdOutput);

        $resultDevices = $zpoolStatus->getAllDevices();

        $this->assertEquals($expectedDevices, $resultDevices);
    }

    /**
     * @dataProvider allDevicesWithExcludesGetterDataProvider
     */
    public function testAllDevicesWithExcludesGetter($cmdOutput, $excludeStates, $expectedDevices) {
        $zpoolStatus = new OMVModuleZFSZpoolStatus($cmdOutput);

        $resultDevices = $zpoolStatus->getAllDevices([
            "excludeStates" => $excludeStates
        ]);

        $this->assertEquals($expectedDevices, $resultDevices);
    }

    public function allDevicesGetterDataProvider() {
        return [
            "simple, healthy pool's output" => [
                Test\OMVZFS\ZpoolStatus\Mocks\SimpleMock::getCmdOutput(),
                [
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                ]
            ],
            "complex pool's output (vdevs, logs & spares)" => [
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getCmdOutput(),
                [
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "12655960456386485198",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                ]
            ]
        ];
    }

    public function allDevicesWithExcludesGetterDataProvider() {
        return [
            "simple, healthy pool's output" => [
                Test\OMVZFS\ZpoolStatus\Mocks\SimpleMock::getCmdOutput(),
                [
                    OMVModuleZFSVdevState::STATE_ONLINE
                ],
                []
            ],
            "complex pool's output (vdevs, logs & spares) (exclude SPARE_INUSE)" => [
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getCmdOutput(),
                [
                    OMVModuleZFSVdevState::STATE_SPARE_INUSE
                ],
                [
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "12655960456386485198",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                ]
            ],
            "complex pool's output (vdevs, logs & spares) (exclude SPARE_INUSE & UNAVAIL)" => [
                Test\OMVZFS\ZpoolStatus\Mocks\AdvancedMock::getCmdOutput(),
                [
                    OMVModuleZFSVdevState::STATE_SPARE_INUSE,
                    OMVModuleZFSVdevState::STATE_UNAVAIL
                ],
                [
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                    "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1"
                ]
            ]
        ];
    }
}
