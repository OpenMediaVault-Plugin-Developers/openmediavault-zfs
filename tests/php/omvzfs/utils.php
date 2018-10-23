<?php

// Relative "base" path used
$filePath = dirname(__FILE__);

class OMVZFSTestUtils {
    /**
     * Loads program's output (stored as a text file) into an array of strings.
     *
     * @param string $filepath
     *  Path to the output's file (with extension).
     *  Uses "omvzfs" directory as the root.
     * @return array
     */
    public static function loadCmdOutputFile(string $filepath) {
        $basepath = dirname(__FILE__);

        $fullpath = $basepath . $filepath;
        $filedata = file_get_contents($fullpath);

        return explode("\n", $filedata);
    }

    /**
     * Loads a JSON file and puts it into a PHP array.
     *
     * @param string $filepath
     *  Path to the JSON file (with extension).
     *  Uses "omvzfs" directory as the root.
     * @return array
     */
    public static function loadJSONFile(string $filepath) {
        $basepath = dirname(__FILE__);

        $fullpath = $basepath . $filepath;
        $filedata = file_get_contents($fullpath);

        return json_decode($filedata, true);
    }
}
