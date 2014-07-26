SyslogParser
============

A system log parser designed to make use of system resources to be as fast as possible.


__Goals__

The goal should be to leverage a systems resources in order to provide a fast way to parse through the system log. This is helpful for things like parsing apparmor denials, iptables violations, application crashes, violations, etc.

The end goal is to have this thing get plugged into an interface and do a couple of cool things, but for now I'm just focusing on getting the system log split up and passed to threads for parsing.

If you notice any areas where code is ugly, inefficient, unsafe, etc. please feel free to let me know or submit a pull request with an explanation behind it.

This is *not* production quality. Don't try to build off of this for something else. I've barely worked on it yet and it could easily break things on your system.

__TODO__

* Remove duplicate rules
* Generate rules based on parsed Apparmor violations
* Parse for iptables rules

__Build Instructions__

```
$ make
```