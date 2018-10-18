<?php

/**
 * Class containing all known Vdev States.
 *
 * For more info, refer to ZOL's:
 * - zpool_state_to_name()
 * - print_status_config() [additional states of "spares"]
 * - vdev_state_t
 * - vdev_aux_t
 *
 * @author    Michal Dziekonski
 * @version   1.0.0
 * @copyright Michal Dziekonski <https://github.com/mdziekon>
 */
abstract class OMVModuleZFSVdevState {
    // Unknown state
    const STATE_UNKNOWN = 0x1;

    // Vdev is either closed or offline
    const STATE_OFFLINE = 0x2;

    // Vdev has been removed
    const STATE_REMOVED = 0x4;

    // Vdev is either in:
    // * "cannot open" state + "data corrupted" aux state
    // * "cannot open" state + "bad log" aux state
    // * "device is faulted"
    const STATE_FAULTED = 0x8;

    // Vdev cannot be opened because it was split into another pool
    const STATE_SPLIT = 0x10;

    // Vdev cannot be opened for another reason
    // Usually, "used" devices' names (data vdevs, caches, logs) will have
    // their name changed to GUID and will have a "was /previous/path" note.
    // However, "spares" that disappeared won't be changed
    // (they will still be identified by their "/previous/path").
    const STATE_UNAVAIL = 0x20;

    // Vdev contains unhealthy descendants
    const STATE_DEGRADED = 0x40;

    // Vdev is healthy
    const STATE_ONLINE = 0x80;

    // Spare vdev is healthy
    const STATE_SPARE_AVAIL = 0x100;

    // Spare is already in use by another pool
    const STATE_SPARE_INUSE = 0x200;

    /**
     * Parses the raw STATE's column string and turns it into one of the constants.
     *
     * @param string $state
     *  The raw value of "STATE" column
     * @return OMVModuleZFSVdevState::STATE_*
     * @throws OMVModuleZFSException
     */
    public static function parseState(string $state) {
        switch ($state) {
            case "OFFLINE":
                return OMVModuleZFSVdevState::STATE_OFFLINE;
            case "REMOVED":
                return OMVModuleZFSVdevState::STATE_REMOVED;
            case "FAULTED":
                return OMVModuleZFSVdevState::STATE_FAULTED;
            case "SPLIT":
                return OMVModuleZFSVdevState::STATE_SPLIT;
            case "UNAVAIL":
                return OMVModuleZFSVdevState::STATE_UNAVAIL;
            case "DEGRADED":
                return OMVModuleZFSVdevState::STATE_DEGRADED;
            case "ONLINE":
                return OMVModuleZFSVdevState::STATE_ONLINE;
            case "AVAIL":
                return OMVModuleZFSVdevState::STATE_SPARE_AVAIL;
            case "INUSE":
                return OMVModuleZFSVdevState::STATE_SPARE_INUSE;
            case "UNKNOWN":
                return OMVModuleZFSVdevState::STATE_UNKNOWN;
            default:
                break;
        }

        throw new OMVModuleZFSException("Unrecognized vdev state string");
    }
}

?>
