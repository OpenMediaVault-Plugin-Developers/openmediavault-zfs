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

        $this->assertEquals($result, $expectedStructure);
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
}
