.PHONY: all cpp 
all: cpp
cpp: frontend_cpp
TARGETS=frontend_tmp Frontend service_test smtp frontend_test

all: $(TARGETS)
frontend_tmp: penncloud.proto
	protoc $$PROTO_PATH --cpp_out=./ penncloud.proto
	@touch frontend_tmp

Frontend_service.o: Frontend_service.cc frontend_tmp
	pkg-config --cflags protobuf
	g++ -c -Wall -g Frontend_service.cc penncloud.pb.cc -o Frontend_service.o `pkg-config --cflags --libs protobuf`

Frontend.o: Frontend.cc frontend_tmp
	pkg-config --cflags protobuf
	g++ -c -Wall -g Frontend.cc penncloud.pb.cc -o Frontend.o `pkg-config --cflags --libs protobuf`

Frontend: Frontend_service.o Frontend.o
	pkg-config --cflags protobuf
	g++ Frontend_service.o Frontend.o -o Frontend `pkg-config --cflags --libs protobuf`

service_test: Frontend_service.cc Frontend.h frontend_tmp
	pkg-config --cflags protobuf
	g++ -g Frontend_service.cc penncloud.pb.cc -Wall -g -o service_test `pkg-config --cflags --libs protobuf`

smtp: Frontend_service.cc smtp.cc Frontend.h frontend_tmp
	pkg-config --cflags protobuf
	g++ Frontend_service.cc penncloud.pb.cc smtp.cc -lresolv -g -Wall -o smtp `pkg-config --cflags --libs protobuf`

frontend_test: Frontend_service.cc Frontend.cc Frontend.h frontend_tmp
	pkg-config --cflags protobuf
	g++ -lpthread Frontend_service.cc penncloud.pb.cc Frontend.cc -lresolv -g -Wall -o frontend_test `pkg-config --cflags --libs protobuf`

clean:
	rm -rf $(TARGETS) *.o
