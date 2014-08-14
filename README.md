SyslogParser
============

A system log parser designed to make use of system resources to be as fast as possible.

In the current stage there's already a clear benefit to the threaded approach. 


__Goals__

The goal should be to leverage a systems resources in order to provide a fast way to parse through the system log. This is helpful for things like parsing apparmor denials, iptables violations, application crashes, violations, etc.

If you notice any areas where code is ugly, inefficient, unsafe, etc. please feel free to let me know or submit a pull request with an explanation behind it.

This is *not* production quality. Don't try to build off of this for something else. I've barely worked on it.

__TODO__

* Remove duplicate apparmor rules
* Start looking for iptables users for output rules

__Build Instructions__

```
$ make
$ make install

run with syslogparse [parameter]
```