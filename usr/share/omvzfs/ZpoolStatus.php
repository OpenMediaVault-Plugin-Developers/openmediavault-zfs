<?php

require_once "Vdev.php";
require_once "VdevType.php";
require_once "VdevState.php";

/**
 * Class containing parsed status of a zpool.
 * In most cases, should be constructed as a member of OMVModuleZFSZpool.
 *
 * @author    Michal Dziekonski
 * @version   1.0.0
 * @copyright Michal Dziekonski <https://github.com/mdziekon>
 */
class OMVModuleZFSZpoolStatus {
    /**
     * The parsed status of a zpool
     *
     * @var    array $status
     * @access private
     */
    private $status;

    public function __construct($statusCmdOutput) {
        $this->status = self::parseStatus($statusCmdOutput);
    }

    /**
     * Return pool's name.
     *
     * @return string
     */
    public function getPoolsName() {
        return $this->status["pool"];
    }

    /**
     * Return pool's state.
     *
     * @return string
     */
    public function getState() {
        return $this->status["state"];
    }

    /**
     * Checks whether the config entry has been properly loaded
     * (as in - it contains useful data instead of an error message).
     *
     * @return boolean
     */
    public function hasConfig() {
        return is_array($this->status["config"]);
    }

    /**
     * Returns all devices (not vdevs!) as absolute paths.
     *
     * @param array $options
     *  Additional options for the getter defined as an associative array:
     *  - "excludeStates" (array)
     *      Vdev states to be excluded from the list
     * @return array
     * @throws OMVModuleZFSException
     * @todo This method should not return file-based vdevs on its list
     */
    public function getAllDevices(array $options = array()) {
        if (!($this->hasConfig())) {
            throw new OMVModuleZFSException("Config could not be loaded");
        }

        $devices = [];

        foreach ($this->status["config"] as $rawVdevs) {
            $vdevDevices = $this->extractDevicesFromVdevs($rawVdevs["subentries"], $options);

            foreach ($vdevDevices as $vdevDevice) {
                $devices[] = $vdevDevice;
            }
        }

        return $devices;
    }

    /**
     * Extracts real devices from top-level vdev's subentries.
     *
     * @param array $options
     *  Additional options for the wrapper defined as an associative array:
     *  - "excludeStates" (array)
     *      Vdev states to be excluded from the list
     * @return array
     * @throws OMVModuleZFSException
     * @todo Once OMVModuleZFSVdevType gets extended with top-level vdev types,
     *       there should be a more general wrapping method.
     */
    private function extractDevicesFromVdevs($rawVdevs, array $options = array()) {
        $allDevices = array();

        // Sanitize options
        if (!array_key_exists("excludeStates", $options)) {
            $options["excludeStates"] = [];
        }

        $vdevSpecialTypes = [
            "mirror",
            "raidz1",
            "raidz2",
            "raidz3"
        ];

        $poolName = $this->getPoolsName();

        foreach ($rawVdevs as $rawVdev) {
            $vdevName = $rawVdev["name"];
            $vdevState = OMVModuleZFSVdevState::parseState($rawVdev["state"]);
            $vdevType = null;

            // Check if vdev is not in the excluded state
            if (in_array($vdevState, $options["excludeStates"], true)) {
                continue;
            }

            $specialTypeDetected = preg_match(
                "/^(" . (implode("|", $vdevSpecialTypes)) . ")/",
                $vdevName,
                $vdevTypeMatch
            );

            if ($specialTypeDetected === 1) {
                switch ($vdevTypeMatch[1]) {
                    case "mirror":
                        $vdevType = OMVModuleZFSVdevType::OMVMODULEZFSMIRROR;
                        break;
                    case "raidz1":
                        $vdevType = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ1;
                        break;
                    case "raidz2":
                        $vdevType = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ2;
                        break;
                    case "raidz3":
                        $vdevType = OMVModuleZFSVdevType::OMVMODULEZFSRAIDZ3;
                        break;
                }
            } else if ($specialTypeDetected === 0) {
                $vdevType = OMVModuleZFSVdevType::OMVMODULEZFSPLAIN;
            } else {
                throw new OMVModuleZFSException("An error occured while detecting vdev type");
            }

            if ($vdevType === OMVModuleZFSVdevType::OMVMODULEZFSPLAIN) {
                $vdevDevices = [ $vdevName ];
            } else {
                // "Virtual devices cannot be nested"
                // ~ZPOOL(8)
                $vdevDevices = [];

                foreach ($rawVdev["subentries"] as $subVdev) {
                    $subVdevState = OMVModuleZFSVdevState::parseState($subVdev["state"]);

                    // Check if device is not in the excluded state
                    if (in_array($subVdevState, $options["excludeStates"], true)) {
                        continue;
                    }

                    $vdevDevices[] = $subVdev["name"];
                }
            }

            // If devices array is empty, do not add this vdev to the list
            // (it means that all devices have been excluded for some reason)
            if (count($vdevDevices) === 0) {
                continue;
            }

            foreach ($vdevDevices as $device) {
                $allDevices[] = $device;
            }
        }

        return $allDevices;
    }

    /**
     * Parse pool's status entries
     * and return it as a nested object.
     *
     * Returns an array of "key => value" pairs, where "key" is the name of an entry
     * (eg. "status" or "scan"), and "value" is it's value. In most cases, "value"
     * is a String containing the whole content of an entry, with all lines merged
     * into a single string (joined with "a space" instead of "new line" character).
     * Some entries may have special parsers for easier data management.
     * Entries known to have dedicated "value" parsers:
     * - "config" (see OMVModuleZFSZpoolStatus::parseStatusConfig)
     *
     * @param array $cmdOutput
     * @return array
     */
    public static function parseStatus($cmdOutput) {
        $statusEntries = array();
        $currentEntryName = "";

        // Group output lines by entry names
        foreach ($cmdOutput as $outputLine) {
            if (strlen($outputLine) === 0) {
                // Empty line, ignore as it does not carry any information.
                continue;
            }

            if ($outputLine[0] === "\t") {
                // Tab indentation usually means the line contains previous message's continuation.
                // Append to the currently processed entry, removing the "\t" character
                // Note: there are some exceptions to this rule, eg.:
                // - verbose errors listing option (which this parser does not handle anyway)
                // (seems like "zpool status"'s code is not very consistent...)
                $statusEntries[$currentEntryName][] = substr($outputLine, 1);

                continue;
            }

            // Try to parse a regular entry line (or multiline entry's beginning)
            $matchReturn = preg_match("/^\s*(.*?):(.*?)$/", $outputLine, $lineMatches);

            if ($matchReturn !== 1) {
                // Line did not match or an error occured,
                // ignore it, because we don't know what to do with it
                continue;
            }

            $entryName = trim($lineMatches[1]);
            $entryValue = trim($lineMatches[2]);

            $currentEntryName = $entryName;

            $statusEntries[$currentEntryName] = array();

            if (strlen($entryValue) !== 0) {
                $statusEntries[$currentEntryName][] = $entryValue;
            }
        }

        // Process entry lines (and use dedicated parsers where needed)
        foreach ($statusEntries as $entryName => $entryLines) {
            if (count($entryLines) === 0) {
                // No lines to parse, replace with "null"
                $statusEntries[$entryName] = null;

                continue;
            }

            $parsedEntryLines = null;

            switch ($entryName) {
                case "config":
                    $parsedEntryLines = OMVModuleZFSZpoolStatus::parseStatusConfig($entryLines);
                    break;
                default:
                    // No dedicated entry parser, concat all lines into a big string
                    // and replace new line character with a space.
                    $parsedEntryLines = implode(" ", $entryLines);
                    break;
            }

            $statusEntries[$entryName] = $parsedEntryLines;
        }

        return $statusEntries;
    }

    /**
     * Parse pool's "config" status entry
     * and return it as a nested object.
     * In case of a known error returns "false",
     * in case of an unrecognized error returns "null".
     *
     * Returns an array of objects matching ConfigEntry pseudo-type.
     * Each ConfigEntry object contains these properties:
     * - "name"
     *      The name (identifier) of the entry. Can be the pool's name,
     *      special vdev group's name (eg. logs, spares),
     *      vdev group's name (eg. mirror-0, raidz2-1) or a device identifier
     *      (either absolute path or its GUID).
     * - "state"
     *      Vdev's state. See "omvzfs/VdevState.php" for well-known states.
     * - "read"
     *      Read errors number.
     * - "write"
     *      Write errors number.
     * - "cksum"
     *      Checksum errors number.
     * - "notes"
     *      Optional strings provided by the "status" command.
     *      An example would be "was /previous/path/to/dev" on moved (unavailable) device.
     * - "subentries"
     *      An array of all "children" entries (ConfigEntry pseudo-type objects).
     *
     * @param array $outputLines
     * @return array
     */
    private static function parseStatusConfig($outputLines) {
        if (count($outputLines) === 1) {
            if (strpos($outputLines[0], "The configuration cannot be determined") !== false) {
                return false;
            }

            // Unrecognized error
            return null;
        }

        $CMD_ZPOOLSTATUS_INDENTSIZE = 2;
        $CMD_ZPOOLSTATUS_SPECIALVDEVS_HEADERS = [
            "spares",
            "logs",
            "cache"
        ];

        $config = array();

        // Nesting stack, containing references to currently modified entries
        // only the top of the stack is a regular object to make it easier to store some flags
        $nestingStack = array();
        $nestingStack[] = array(
            "subentries" => &$config
        );
        $nestingIndentation = 0;
        $isParsingSpares = false;
        $currentNestingEntry = &$nestingStack[count($nestingStack) - 1];

        foreach ($outputLines as $outputLine) {
            if (preg_match("/^NAME/", $outputLine)) {
                // This is the table's header, ignore it.
                continue;
            }

            // Calculate current indentation
            $trimmedLine = ltrim($outputLine);
            $currentIndentation = strlen($outputLine) - strlen($trimmedLine);

            if ($currentIndentation > $nestingIndentation) {
                // Nesting has deepened,
                // create new nested structure and append reference to the stack

                $lastSubentryIdx = count($currentNestingEntry["subentries"]) - 1;
                $nestingStack[] = &$currentNestingEntry["subentries"][$lastSubentryIdx];
                $currentNestingEntry = &$nestingStack[count($nestingStack) - 1];
                $nestingIndentation = $currentIndentation;
            } else if ($currentIndentation < $nestingIndentation) {
                // Went back from the previous nesting level,
                // pop the last reference from the stack and update local vars

                // Detect the indentation difference
                $levelsUp = (
                    ($nestingIndentation - $currentIndentation) /
                    $CMD_ZPOOLSTATUS_INDENTSIZE
                );

                while ($levelsUp > 0) {
                    array_pop($nestingStack);

                    $levelsUp--;
                }

                $currentNestingEntry = &$nestingStack[count($nestingStack) - 1];
                $nestingIndentation = $currentIndentation;
            }

            // Split the line into columns
            $trimmedLine = trim($outputLine);
            $lineDetails = preg_split("/\s+/", $trimmedLine);

            // Create the new entry with basic info:
            // - "name"
            //      Column is always present.
            // - "state"
            //      Column is present when parsing vdevs (including root vdev).
            //      Alternatively, it is NOT present when parsing special vdevs
            //      group headers.
            // - "read", "write", "cksum"
            //      Columns are present when parsing vdevs (including root vdev),
            //      except for spare vdevs.
            //      Alternatively, it is NOT present when parsing special vdevs
            //      group headers and spare vdevs.
            // - "notes" [virtual, no top-level column]
            //      Column is present mostly when something bad happens
            //      with the vdev's entry.
            $columnsReadCount = 0;

            $newEntry = array(
                "name" => $lineDetails[0],
                "state" => null,

                "read" => null,
                "write" => null,
                "cksum" => null,

                "notes" => null,

                "subentries" => array()
            );

            $columnsReadCount += 1;

            $isParsingSpecialVdevsGroupHeader = in_array(
                $newEntry["name"],
                $CMD_ZPOOLSTATUS_SPECIALVDEVS_HEADERS
            );
            $isParsingVdev = (!$isParsingSpecialVdevsGroupHeader);
            // "spare" vdev type is determined by the top-level entry called "spares",
            // which means that we can "enter" spares parsing only when reached
            // descendants of "spares" entry, and we can "leave" spares parsing
            // when the parsing stack has cleared (to the top-level stack frame entry).
            $isParsingSpares = (
                ($newEntry["name"] === "spares") ||
                (
                    $isParsingSpares &&
                    count($nestingStack) > 1
                )
            );

            $hasStateColumn = ($isParsingVdev);
            $hasStatsColumns = (
                $isParsingVdev &&
                !$isParsingSpares
            );
            $canHaveNotesColumn = (!$isParsingSpecialVdevsGroupHeader);

            if ($hasStateColumn) {
                $newEntry["state"] = $lineDetails[1];

                $columnsReadCount += 1;
            }

            if ($hasStatsColumns) {
                $newEntry["read"] = $lineDetails[2];
                $newEntry["write"] = $lineDetails[3];
                $newEntry["cksum"] = $lineDetails[4];

                $columnsReadCount += 3;
            }

            if ($canHaveNotesColumn) {
                // Read the rest as a whole column
                // (without losing the original whitespacing)
                // (use non-capturing group to go past the already-read columns)
                $matchReturn = preg_match(
                    "/^(?:\S+\s+){{$columnsReadCount}}(.*?)$/",
                    $trimmedLine,
                    $restMatch
                );

                if ($matchReturn === 1) {
                    $newEntry["notes"] = $restMatch[1];
                }
            }

            $currentNestingEntry["subentries"][] = $newEntry;
        }

        return $config;
    }
}
?>
