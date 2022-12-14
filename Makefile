.PHONY=clean all
CC=gcc
CFLAGS = -g -Wall -pedantic  
LDLIBS = -pthread -lm -lrt

ALL = test1 test2

all: $(ALL)

m_file.o: m_file.c m_file.h
	$(CC) $(CFLAGS) -c m_file.c $(LDLIBS)



test1.o : m_file.c test1.c m_file.h
	$(CC) $(CFLAGS) -c test1.c $(LDLIBS)

test1: test1.o m_file.o
	$(CC) $(CFLAGS) test1.o m_file.o -o test1 $(LDLIBS)

test2.o : m_file.c test2.c m_file.h
	$(CC) $(CFLAGS) -c test2.c $(LDLIBS)

test2: test2.o m_file.o
	$(CC) $(CFLAGS) test2.o m_file.o -o test2 $(LDLIBS)

# m_file: m_file.o 
# m_file.o: m_file.c 

clean: 
	rm *.o m_file
	