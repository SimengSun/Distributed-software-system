TARGETS = frontendmaster

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@

frontendmaster: frontendmaster.o
	g++ $^ -std=c++11 -lpthread  -o $@

pack:

clean::
	rm -fv $(TARGETS) *~ *.o submit-hw3.zip

realclean:: clean
	rm -fv cis505-hw3.zip
