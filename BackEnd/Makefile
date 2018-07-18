TARGETS = backend

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

backend: backend.o ../Bigtable/Bigtable.o
	g++ $^ -std=c++11 -lpthread -g -o$@

pack:

clean::
	rm -fv $(TARGETS) *~ *.o submit-hw3.zip ../Bigtable/Bigtable.o

realclean:: clean
	rm -fv cis505-hw3.zip
