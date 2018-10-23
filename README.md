openmediavault-zfs
==================

### Commands:

- build .deb package:  
  ``fakeroot debian/rules clean binary``
- install PHP dependencies:  
  ``composer install``
- run PHP tests:  
  ``composer run-script test``

### Objects:

```
- zpool
    - vdev
    - disk
- zvol
- dataset
- snapshot

<zpool>		::= <vdev> <log> <cache> <spare> | <vdev> <log> <cache> <spare> <zpool>;
<vdev>		::= <diskset> | <mirror> | <raidz1> | <raidz2> | <raidz3>;
<mirror>	::= "disk" "disk" | "disk" <mirror>;
<raidz1>	::= "disk" "disk" "disk" | "disk" <raidz1>;
<raidz2>	::= "disk" "disk" "disk" "disk" | "disk" <raidz2>;
<raidz3>	::= "disk" "disk" "disk" "disk" "disk" | "disk" <raidz3>;
<log>		::= "internal" | <diskset> | <mirror>;
<cache>		::=	"internal" | <diskset>;
<spare>		::= "" | <diskset>;
<diskset>	::= "disk" | "disk" <diskset>;
```
