CC=g++
CFLAGS=-Wall -g -c -lm -pthread
TARGETS=Bigtabletest

all: $(TARGETS)

Bigtabletest: Bigtable/test.o Bigtable/Bigtable.h Bigtable/Bigtable.o
	$(CC) Bigtable/test.o Bigtable/Bigtable.o -o $@

Bigtable/test.o: Bigtable/test.cc Bigtable/Bigtable.h
	$(CC) $(CFLAGS) Bigtable/test.cc -o $@

Bigtable/Bigtable.o: Bigtable/Bigtable.cc Bigtable/Bigtable.h
	$(CC) $(CFLAGS) Bigtable/Bigtable.cc -o $@

lsLOG: Bigtable/ls_LOG.o Bigtable/Bigtable.o
	$(CC) Bigtable/ls_LOG.o Bigtable/Bigtable.o -o $@

Bigtable/ls_LOG.o: Bigtable/ls_LOG.cc Bigtable/Bigtable.h
	$(CC) $(CFLAGS) Bigtable/ls_LOG.cc -o Bigtable/ls_LOG.o

.PHONY: clean

clean:
	rm -rf $(TARGETS) *.o Bigtable/*.o