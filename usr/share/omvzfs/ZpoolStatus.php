<?php

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
}
?>
