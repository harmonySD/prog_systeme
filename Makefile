.PHONY=clean all
CC=gcc
CFLAGS= -g -Wall -pedantic -pthread
LDLIBS=-lm -lrt

ALL = m_file

all: $(ALL)

m_file.o: m_file.c m_file.h
	$(CC) $(CFLAGS) -c m_file.c

m_file: m_file.o 
	$(CC) $(CFLAGS) m_file.o -o m_file

# m_file: m_file.o 
# m_file.o: m_file.c 

clean: 
	rm *.o m_file
	