.PHONY=clean all
CC=gcc
CFLAGS = -g -Wall -pedantic  
LDLIBS = -pthread -lm -lrt

ALL = test1

all: $(ALL)

m_file.o: m_file.c m_file.h
	$(CC) $(CFLAGS) -c m_file.c $(LDLIBS)



test1.o : m_file.c test1.c m_file.h
	$(CC) $(CFLAGS) -c test1.c $(LDLIBS)

test1: test1.o m_file.o
	$(CC) $(CFLAGS) test1.o m_file.o -o test1 $(LDLIBS)

# m_file: m_file.o 
# m_file.o: m_file.c 

clean: 
	rm *.o m_file
	