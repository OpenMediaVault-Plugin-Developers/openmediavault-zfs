<?php
namespace OMV\System\Filesystem\Backend;
require_once("/usr/share/omvzfs/Utils.php");
require_once("/usr/share/omvzfs/Filesystem.php");

class Zfs extends BackendAbstract
{
    public function __construct()
    {
        $this->type = "zfs";
        $this->properties = self::PROP_POSIX_ACL;
    }

    public function getImpl($args)
    {
        $object = new \OMV\System\Filesystem\FilesystemZfs($args);
        $object->setBackend($this);
        return $object;
    }

    public function isTypeOf($fsName)
    {
        $cmd = "zfs list -H -o name -t filesystem \"$fsName\"";
        OMVModuleZFSUtil::exec($cmd, $out, $res);
        if ($res == 0)
            return TRUE;
        return FALSE;
    }

    public function enumerate()
    {
        $filesystems = \OMVModuleZFSFilesystem::getAllFilesystems();
        $result = [];
        foreach ($filesystems as $filesystem) {
            $name = $filesystem->getName();
            $data = [
                "devicefile" => $name,
                "uuid" => "",
                "label" => $name,
                "type" => "zfs"
            ];
            $result[$name] = $data;
        }
        return $result;
    }

    public function isBlkidEnumerated()
    {
        return FALSE;
    }
}