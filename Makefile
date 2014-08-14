## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE

syslogparse : syslogparse.o
	g++ syslogparse.o -g -O2 --std=c++0x -Wall -Wextra -pedantic -pthread -fPIC -fPIE -fstack-protector-all -FORTIFY_SOURCE -o syslogparse


## caller : caller.o
##	g++ caller.o -O2 --std=c++0x -Wextra -pedantic -o caller

syslogparse.o : syslogparse.cpp
##caller.o : caller.cpp 

make all:

	make syslogparse
	rm *.o

make install:
	mv syslogparse /usr/bin/syslogparse

clean:

	rm *.o syslogparse

## eof Makefile