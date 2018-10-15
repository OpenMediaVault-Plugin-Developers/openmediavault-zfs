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
     * @return array
     * @throws OMVModuleZFSException
     * @todo This method should not return file-based vdevs on its list
     */
    public function getAllDevices() {
        if (!($this->hasConfig())) {
            throw new OMVModuleZFSException("Config could not be loaded");
        }

        $devices = [];

        foreach ($this->status["config"] as $rawVDevs) {
            $vdevs = $this->wrapVDevs($rawVDevs["subentries"]);

            foreach ($vdevs as $vdev) {
                // TODO: Append only ONLINE vdevs
                foreach ($vdev->getDisks() as $newDevice) {
                    $devices[] = $newDevice;
                }
            }
        }

        return $devices;
    }

    /**
     * Wraps raw vdevs information into OMVModuleZFSVdev instances.
     * Expects to receive an array of "subentries" taken from top-level vdev
     * (root-vdev, logs, cache, spares...)
     *
     * @param array $options
     *  Additional options for the wrapper defined as an associative array:
     *  - "excludeStates" (array)
     *      Vdev states to be excluded from the list
     * @return array
     * @throws OMVModuleZFSException
     * @todo Once OMVModuleZFSVdevType gets extended with these top-level
     *       vdev types, there should be a more general wrapping method.
     */
    private function wrapVDevs($rawVDevs, array $options = array()) {
        $vdevs = array();

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

        foreach ($rawVDevs as $rawVDev) {
            $vdevName = $rawVDev["name"];
            $vdevState = OMVModuleZFSVdevState::parseState($rawVDev["state"]);
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

                foreach ($rawVDev["subentries"] as $subVDev) {
                    $subVDevState = OMVModuleZFSVdevState::parseState($subVDev["state"]);

                    // Check if device is not in the excluded state
                    if (in_array($subVDevState, $options["excludeStates"], true)) {
                        continue;
                    }

                    $vdevDevices[] = $subVDev["name"];
                }
            }

            // If devices array is empty, do not add this vdev to the list
            // (it means that all devices have been excluded for some reason)
            if (count($vdevDevices) === 0) {
                continue;
            }

            $vdev = new OMVModuleZFSVdev($poolName, $vdevType, $vdevDevices);

            $vdevs[] = $vdev;
        }

        return $vdevs;
    }

    /**
     * Parse pool's status entries
     * and return it as a nested object.
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
                    // and preserve new line characters
                    $parsedEntryLines = implode("\n", $entryLines);
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

                $nestingStack[] = &$currentNestingEntry["subentries"][count($currentNestingEntry["subentries"]) - 1];
                $currentNestingEntry = &$nestingStack[count($nestingStack) - 1];
                $nestingIndentation = $currentIndentation;
            } else if ($currentIndentation < $nestingIndentation) {
                // Went back from the previous nesting level,
                // pop the last reference from the stack and update local vars

                // Detect the indentation difference
                $levelsUp = ($nestingIndentation - $currentIndentation) / $CMD_ZPOOLSTATUS_INDENTSIZE;

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

            // Create the new entry with basic info
            // "name" & "state" columns are always present
            $columnsReadCount = 0;

            $newEntry = array(
                "name" => $lineDetails[0],
                "state" => $lineDetails[1],

                "read" => null,
                "write" => null,
                "cksum" => null,

                "notes" => null,

                "subentries" => array()
            );

            $columnsReadCount += 2;

            // "read", "write" & "cksum" are not shown for "spares",
            // so we have to determine if we're parsing a "spare" entry upfront.
            // "spare" type is determined by the top-level entry called "spares",
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

            // "not" spares will have three additional columns
            if (!$isParsingSpares) {
                $newEntry["read"] = $lineDetails[2];
                $newEntry["write"] = $lineDetails[3];
                $newEntry["cksum"] = $lineDetails[4];

                $columnsReadCount += 3;
            }

            // Read the rest as a whole column
            // (without losing the original whitespacing)
            // (use non-capturing group to go past the already-read columns)
            $matchReturn = preg_match("/^(?:\S+\s+){{$columnsReadCount}}(.*?)$/", $trimmedLine, $restMatch);

            if ($matchReturn === 1) {
                $newEntry["notes"] = $restMatch[1];
            }

            $currentNestingEntry["subentries"][] = $newEntry;
        }

        return $config;
    }
}
?>
