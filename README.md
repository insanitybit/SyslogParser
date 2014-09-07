SyslogParser
============

A system log parser designed to make use of system resources to be as fast as possible.

This is a largely complete project. I'll do little things to it, but it does what I needed it to do, and building a GUI around it doesn't make sense. I'll be building something much more worthwhlie instead.

The apparmor tools are totally broken on every system I've used. They break very easily when you try to sandbox programs like Chrome or Python. This is less complete than a tool like aa-logprof, which will insert the rules on top of what syslogparse does.

__Features__

* Threaded where possible, performance increase is considerable with even a dual core CPU.
* Generation of iptables and apparmor rules.
* Makes use of sandboxing. Chroot, capabilities, DAC, MAC, seccomp.

__Goals__

The goal of this program is to sort through the system log, find iptables and apparmor violations, and generate usable rules based on them.

All of this is done within a threaded, sandboxed process.


__Build Instructions__

```
$ make all
$ sudo make install

run with syslogparse [parameter]

requires libseccomp and libcap-ng. Both can be installed by apt or compiled from source.
```
