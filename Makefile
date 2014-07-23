## file Makefile

CXXOBJECTS= %.o
CXX= g++ 
CXXSOURCES= syslogparse.cpp
CXXFLAGS= -O0 --std=c++0x -Wextra -pedantic
#I really have to figure out when to send in these flags

syslogparse : syslogparse.o
	g++ syslogparse.o -O0 --std=c++0x -Wextra -pedantic -pthread -o syslogparse


syslogparse.o : syslogparse.cpp
 
clean:

	rm *.o

## eof Makefile