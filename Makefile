## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE -lseccomp

syslogparse : syslogparse.o
	g++ syslogparse.o -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE -lseccomp -o syslogparse


## caller : caller.o
##	g++ caller.o -O2 --std=c++0x -Wextra -pedantic -o caller

syslogparse.o : syslogparse.cpp
##caller.o : caller.cpp 

make all:

	make syslogparse
	rm -f *.o

make install:
	cp syslogparse /usr/bin/syslogparse
	cp usr.bin.syslogparse /etc/apparmor.d/usr.bin.syslogparse
make uninstall:
	rm -f /usr/bin/syslogparse

clean:

	rm -f *.o syslogparse

## eof Makefile