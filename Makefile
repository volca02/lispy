CXXFLAGS=-std=c++11 -ggdb -O0 -Wall
LDFLAGS=-lreadline

lispy: lispy.cc lispy.h

clean:
	rm lispy
