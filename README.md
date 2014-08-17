SyslogParser
============

A system log parser designed to make use of system resources to be as fast as possible.

__Features__

* Threaded where possible, performance increase is considerable with even a dual core CPU.
* Generation of iptables and apparmor rules.
* Makes use of sandboxing. Currently seccomp sandboxing and an apparmor profile.

__Goals__

The goal of this program is to sort through the system log, find iptables and apparmor violations, and generate usable rules based on them.

All of this is done within a threaded, sandboxed process. A parent process will display the information in a GUI.


__TODO__

* Work on further sandboxing.
* Build GUI to pipe in rules from child and allow the user to use the rules. Work in progress.
* Validate rules on parent side - child is in apparmor profile, don't allow modification of that profile. Not started.

__Build Instructions__

```
$ make all
$ sudo make install

run with syslogparse [parameter]
```