## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE -lseccomp

syslogparse : syslogparse.o
	g++ syslogparse.o -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE -lseccomp -o syslogparse

syslogparse.o : syslogparse.cpp

all:
	cppcheck --enable=all syslogparse.cpp
	valac --pkg gtk+-3.0 caller.vala
	make syslogparse
	rm -f *.o

install:
	cp syslogparse /usr/bin/syslogparse
	cp usr.bin.syslogparse /etc/apparmor.d/usr.bin.syslogparse

uninstall:
	rm -f /usr/bin/syslogparse

clean:

	rm -f *.o syslogparse

## eof Makefile