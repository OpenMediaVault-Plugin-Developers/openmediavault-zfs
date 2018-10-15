<?php

namespace Test\OMVZFS\ZpoolStatus\Mocks;

interface CmdOutputMockInterface {
    public static function getCmdOutput();

    public static function getExpectedParsedStructure();
}

class SimpleMock implements CmdOutputMockInterface {
    public static function getCmdOutput() {
        return [
            "  pool: testpool",
            " state: ONLINE",
            "  scan: scrub repaired 0B in 0h0m with 0 errors on Sun Oct 14 16:08:26 2018",
            "config:",
            "",
            "	NAME                                                             STATE     READ WRITE CKSUM",
            "	testpool                                                         ONLINE       0     0     0",
            "	  mirror-0                                                       ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1  ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1  ONLINE       0     0     0",
            "",
            "errors: No known data errors"
        ];
    }

    public static function getExpectedParsedStructure() {
        return [
            "pool" => "testpool",
            "state" => "ONLINE",
            "scan" => "scrub repaired 0B in 0h0m with 0 errors on Sun Oct 14 16:08:26 2018",
            "config" => [
                [
                    "name" => "testpool",
                    "state" => "ONLINE",
                    "read" => "0",
                    "write" => "0",
                    "cksum" => "0",
                    "notes" => null,
                    "subentries" => [
                        [
                            "name" => "mirror-0",
                            "state" => "ONLINE",
                            "read" => "0",
                            "write" => "0",
                            "cksum" => "0",
                            "notes" => null,
                            "subentries" => [
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ]
                            ]
                        ]
                    ]
                ]
            ],
            "errors" => "No known data errors"
        ];
    }
}

class AdvancedMock implements CmdOutputMockInterface {
    public static function getCmdOutput() {
        return [
            "  pool: testpool",
            " state: DEGRADED",
            "status: One or more devices could not be used because the label is missing or",
            "	invalid.  Sufficient replicas exist for the pool to continue",
            "	functioning in a degraded state.",
            "action: Replace the device using 'zpool replace'.",
            "   see: http://zfsonlinux.org/msg/ZFS-8000-4J",
            "  scan: scrub repaired 0B in 0h0m with 0 errors on Sun Oct 14 16:08:26 2018",
            "config:",
            "",
            "	NAME                                                             STATE     READ WRITE CKSUM",
            "	testpool                                                         ONLINE       0     0     0",
            "	  mirror-0                                                       ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1  ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1  ONLINE       0     0     0",
            "	  raidz1-0                                                       ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2  ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2  ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2  ONLINE       0     0     0",
            "	spares                                                           ONLINE",
            "	  /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1    INUSE  in use by pool 'otherpool'",
            "	  /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1    AVAIL",
            "	logs                                                             DEGRADED     0     0     0",
            "	  mirror-0                                                       DEGRADED     0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1  ONLINE       0     0     0",
            "	    12655960456386485198                                         UNAVAIL      0     0     0  was /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
            "	  mirror-1                                                       ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1  ONLINE       0     0     0",
            "	    /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1  ONLINE       0     0     0",
            "",
            "errors: No known data errors"
        ];
    }

    public static function getExpectedParsedStructure() {
        return [
            "pool" => "testpool",
            "state" => "DEGRADED",
            "status" => (
                "One or more devices could not be used because the label is missing or" . "\n" .
                "invalid.  Sufficient replicas exist for the pool to continue" . "\n" .
                "functioning in a degraded state."
            ),
            "action" => "Replace the device using 'zpool replace'.",
            "see" => "http://zfsonlinux.org/msg/ZFS-8000-4J",
            "scan" => "scrub repaired 0B in 0h0m with 0 errors on Sun Oct 14 16:08:26 2018",
            "config" => [
                [
                    "name" => "testpool",
                    "state" => "ONLINE",
                    "read" => "0",
                    "write" => "0",
                    "cksum" => "0",
                    "notes" => null,
                    "subentries" => [
                        [
                            "name" => "mirror-0",
                            "state" => "ONLINE",
                            "read" => "0",
                            "write" => "0",
                            "cksum" => "0",
                            "notes" => null,
                            "subentries" => [
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ]
                            ]
                        ],
                        [
                            "name" => "raidz1-0",
                            "state" => "ONLINE",
                            "read" => "0",
                            "write" => "0",
                            "cksum" => "0",
                            "notes" => null,
                            "subentries" => [
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part2",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part2",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-a6c3b929-part2",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ]
                            ]
                        ]
                    ]
                ],
                [
                    "name" => "spares",
                    "state" => "ONLINE",
                    "read" => null,
                    "write" => null,
                    "cksum" => null,
                    "notes" => null,
                    "subentries" => [
                        [
                            "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                            "state" => "INUSE",
                            "read" => null,
                            "write" => null,
                            "cksum" => null,
                            "notes" => "in use by pool 'otherpool'",
                            "subentries" => []
                        ],
                        [
                            "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                            "state" => "AVAIL",
                            "read" => null,
                            "write" => null,
                            "cksum" => null,
                            "notes" => null,
                            "subentries" => []
                        ]
                    ]
                ],
                [
                    "name" => "logs",
                    "state" => "DEGRADED",
                    "read" => "0",
                    "write" => "0",
                    "cksum" => "0",
                    "notes" => null,
                    "subentries" => [
                        [
                            "name" => "mirror-0",
                            "state" => "DEGRADED",
                            "read" => "0",
                            "write" => "0",
                            "cksum" => "0",
                            "notes" => null,
                            "subentries" => [
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "12655960456386485198",
                                    "state" => "UNAVAIL",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => "was /dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                                    "subentries" => []
                                ]
                            ]
                        ],
                        [
                            "name" => "mirror-1",
                            "state" => "ONLINE",
                            "read" => "0",
                            "write" => "0",
                            "cksum" => "0",
                            "notes" => null,
                            "subentries" => [
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VB84ca63f0-93542ba0-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ],
                                [
                                    "name" => "/dev/disk/by-id/ata-VBOX_HARDDISK_VBefaa040c-71d3f044-part1",
                                    "state" => "ONLINE",
                                    "read" => "0",
                                    "write" => "0",
                                    "cksum" => "0",
                                    "notes" => null,
                                    "subentries" => []
                                ]
                            ]
                        ]
                    ]
                ],
            ],
            "errors" => "No known data errors"
        ];
    }
}

?>
