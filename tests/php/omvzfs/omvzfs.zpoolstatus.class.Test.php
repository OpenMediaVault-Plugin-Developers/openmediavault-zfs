<?php

// Relative "base" path used
$filePath = dirname(__FILE__);

require_once $filePath . "/utils.php";
require_once $filePath . "/../../../usr/share/omvzfs/ZpoolStatus.php";
require_once $filePath . "/../../../usr/share/omvzfs/VdevState.php";

class OMVZFSZpoolStatusClassTest extends \PHPUnit\Framework\TestCase {
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
        $mocks = [
            // Simple cases where devices are either referenced as:
            // * /dev/sdXY
            // * /dev/nvme0nXpY
            // * /dev/disk/by-id
            "cmd_output.zpool_status.simplepool_sata_bydev",
            "cmd_output.zpool_status.simplepool_sata_byid",
            "cmd_output.zpool_status.simplepool_nvme_bydev",
            "cmd_output.zpool_status.simplepool_nvme_byid",
            "cmd_output.zpool_status.simplepool_devicemapper_bydev",
            "cmd_output.zpool_status.simplepool_devicemapper_byid",
            "cmd_output.zpool_status.simplepool_mixed_bydev",
            "cmd_output.zpool_status.simplepool_mixed_byid",
            "cmd_output.zpool_status.simplepool_mixed_refsmixed",
            // A bit longer list with more than one nested groups
            "cmd_output.zpool_status.redundantpool_raidz3_double",
            "cmd_output.zpool_status.redundantpool_mixed",
            // Complex pool structure, with caches, logs & spares
            "cmd_output.zpool_status.complexpool_createdupfront",
            // Degraded pools with missing devices
            // (where device GUID is displayed instead of its path)
            "cmd_output.zpool_status.degradedpool_simplemirror",
            "cmd_output.zpool_status.degradedpool_multidegradation",
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
                    "/expectations/omvzfs.zpoolstatus/all_devices/" . $mockName . ".json"
                )
            ];
        }

        return $datasets;
    }

    public function allDevicesWithExcludesGetterDataProvider() {
        $testcases = [
            // Simple case, with no devices excluded (because all are ONLINE)
            "cmd_output.zpool_status.simplepool_sata_bydev (excluded: UNAVAIL)" => [
                "mockfile" => "cmd_output.zpool_status.simplepool_sata_bydev",
                "expectationfile" => "cmd_output.zpool_status.simplepool_sata_bydev.exclude_unavail",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_UNAVAIL
                ]
            ],
            // Simple case, with all devices excluded (because all are ONLINE)
            "cmd_output.zpool_status.simplepool_sata_bydev (excluded: ONLINE)" => [
                "mockfile" => "cmd_output.zpool_status.simplepool_sata_bydev",
                "expectationfile" => "cmd_output.zpool_status.simplepool_sata_bydev.exclude_online",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_ONLINE
                ]
            ],
            // Complex case, with no devices excluded (because none are OFFLINE)
            "cmd_output.zpool_status.complexpool_createdupfront (excluded: OFFLINE)" => [
                "mockfile" => "cmd_output.zpool_status.complexpool_createdupfront",
                "expectationfile" => "cmd_output.zpool_status.complexpool_createdupfront.exclude_offline",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_OFFLINE
                ]
            ],
            // Complex case, with all spares excluded (because all spares are AVAIL)
            "cmd_output.zpool_status.complexpool_createdupfront (excluded: SPARE_AVAIL)" => [
                "mockfile" => "cmd_output.zpool_status.complexpool_createdupfront",
                "expectationfile" => "cmd_output.zpool_status.complexpool_createdupfront.exclude_spare_avail",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_SPARE_AVAIL
                ]
            ],
            // Complex case, with all devices excluded (both SPARE_AVAIL & ONLINE are excluded)
            "cmd_output.zpool_status.complexpool_createdupfront (excluded: SPARE_AVAIL & ONLINE)" => [
                "mockfile" => "cmd_output.zpool_status.complexpool_createdupfront",
                "expectationfile" => "cmd_output.zpool_status.complexpool_createdupfront.exclude_spare_avail_online",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_SPARE_AVAIL,
                    OMVModuleZFSVdevState::STATE_ONLINE
                ]
            ],
            // Degraded simple pool case, with no devices excluded (because none are OFFLINE)
            "cmd_output.zpool_status.degradedpool_simplemirror (excluded: OFFLINE)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_simplemirror",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_simplemirror.exclude_offline",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_OFFLINE
                ]
            ],
            // Degraded simple pool case, with some devices excluded (UNAVAIL ones)
            "cmd_output.zpool_status.degradedpool_simplemirror (excluded: UNAVAIL)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_simplemirror",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_simplemirror.exclude_unavail",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_UNAVAIL
                ]
            ],
            // Degraded simple pool case, with all devices excluded (both UNAVAIL & ONLINE are excluded)
            "cmd_output.zpool_status.degradedpool_simplemirror (excluded: UNAVAIL & ONLINE)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_simplemirror",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_simplemirror.exclude_unavail_online",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_UNAVAIL,
                    OMVModuleZFSVdevState::STATE_ONLINE
                ]
            ],
            // Degraded multidegradation pool case, with no devices excluded (because none are OFFLINE)
            "cmd_output.zpool_status.degradedpool_multidegradation (excluded: OFFLINE)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_multidegradation",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_multidegradation.exclude_offline",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_OFFLINE
                ]
            ],
            // Degraded multidegradation pool case, with some devices excluded (UNAVAIL ones)
            "cmd_output.zpool_status.degradedpool_multidegradation (excluded: UNAVAIL)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_multidegradation",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_multidegradation.exclude_unavail",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_UNAVAIL
                ]
            ],
            // Degraded multidegradation pool case, with some devices excluded (UNAVAIL & SPARE_AVAIL ones)
            "cmd_output.zpool_status.degradedpool_multidegradation (excluded: UNAVAIL & SPARE_AVAIL)" => [
                "mockfile" => "cmd_output.zpool_status.degradedpool_multidegradation",
                "expectationfile" => "cmd_output.zpool_status.degradedpool_multidegradation.exclude_unavail_spare_avail",
                "excludedStates" => [
                    OMVModuleZFSVdevState::STATE_UNAVAIL,
                    OMVModuleZFSVdevState::STATE_SPARE_AVAIL
                ]
            ],
        ];

        $datasets = [];

        foreach ($testcases as $testcaseName => $testcaseConfig) {
            $datasets[$testcaseName] = [
                // testcase mock file content
                OMVZFSTestUtils::loadCmdOutputFile(
                    "/mocks/omvzfs.zpoolstatus/" . $testcaseConfig["mockfile"] . ".txt"
                ),
                // testcase exclusion table
                $testcaseConfig["excludedStates"],
                // testcase mock expected structure
                OMVZFSTestUtils::loadJSONFile(
                    "/expectations/omvzfs.zpoolstatus/all_devices_with_exclusion/" . $testcaseConfig["expectationfile"] . ".json"
                )
            ];
        }

        return $datasets;
    }
}
