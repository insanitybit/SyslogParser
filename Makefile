## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -O3 -mfpmath=sse -msse2 --std=c++0x -Wall -Wextra -pedantic -pthread
CXXFLAGS+= -Wl,-z,relro,-z,now,-z,noexecstack -fPIC -pie -fPIE -fstack-protector-all -lseccomp -lcap-ng


syslogparse : syslogparse.o
	g++ syslogparse.o -O3 -mfpmath=sse -msse2 --std=c++0x -Wall -Wextra -pedantic -pthread -Wl,-z,relro,-z,now,-z,noexecstack -fPIC -pie -fPIE -fstack-protector-all -lseccomp -lcap-ng -o syslogparse

syslogparse.o : syslogparse.cpp

all:
	#### cppcheck --enable=all syslogparse.cpp
	
	make syslogparse

	rm -f *.o

install:
	cp syslogparse /usr/bin/syslogparse
	cp usr.bin.syslogparse /etc/apparmor.d/usr.bin.syslogparse

uninstall:
	rm -f /usr/bin/syslogparse
	rm -f /etc/apparmor.d/usr.bin.syslogparse

clean:

	rm -f *.o syslogparse

## eof Makefile