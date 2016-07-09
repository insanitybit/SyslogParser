SyslogParser
============

A system log parser designed to make use of system resources to be as fast as possible.

This is a largely complete project. I'll do little things to it, but it does what I needed it to do, and building a GUI around it doesn't make sense.

__Goals__

The goal of this program is to sort through the system log, find iptables and apparmor violations, and generate usable rules based on them.

All of this is done within a threaded, sandboxed process.


__Build Instructions__

```
$ make all
$ sudo make install

run with syslogparse [parameter] [path] 

requires libseccomp and libcap-ng. Both can be installed by apt or compiled from source.
```
