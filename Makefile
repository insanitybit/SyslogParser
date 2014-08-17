## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread
CXXFLAGS+= -Wl,-z,relro,-z,now,-z,noexecstack -fPIC -pie -fPIE -fstack-protector-all -lseccomp


syslogparse : syslogparse.o
	g++ syslogparse.o -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -Wl,-z,relro,-z,now,-z,noexecstack -fPIC -pie -fPIE -fstack-protector-all -lseccomp -o syslogparse

syslogparse.o : syslogparse.cpp

all:
	cppcheck --enable=all syslogparse.cpp
	valac --enable-checking --pkg gtk+-3.0 caller.vala -X -O2 -X -fPIC -X -pie -X -fPIE -X -fstack-protector-all -X -Wl,-z,relro,-z,now,-z,noexecstack
	make syslogparse
	rm -f *.o

install:
	cp syslogparse /usr/bin/syslogparse
	cp usr.bin.syslogparse /etc/apparmor.d/usr.bin.syslogparse

uninstall:
	rm -f /usr/bin/syslogparse

clean:

	rm -f *.o syslogparse caller caller.c

## eof Makefile