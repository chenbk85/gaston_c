INC_PROTOBUF       = -I/usr/local/protobuf/include -I./ 
INC_LIBEVENT       = -I/usr/local/libevent/include -L/usr/local/libevent/lib
PROTOBUF_LIBFILE   = /usr/local/protobuf/lib/libprotobuf.a
LIB_SQLPARAM	   = ./libsqlparam.a
LIB_RESULT		   = ./libresult.a
LIB_PROTOBUF       = $(PROTOBUF_LIBFILE) -lpthread 
INC_MYSQL		   = -I/usr/include/mysql
LIB_MYSQL		   = -L/usr/lib/mysql/ `mysql_config --cflags --libs`
COMMON_PATH		   = $(LIB_SQLPARAM) $(LIB_RESULT) $(LIB_PROTOBUF) $(INC_PROTOBUF) $(INC_MYSQL) $(LIB_MYSQL)

all: server client
server: libsqlparam libsqlparam
	g++ -g server.cpp -o server $(COMMON_PATH)
epollfork:
	g++ -g epollfork.cpp -o epollfork $(COMMON_PATH)	
fork: libsqlparam libresult
	g++ -g $(INC_LIBEVENT) fork.cpp -o fork $(COMMON_PATH)	
fork2: 
	g++ -g $(INC_LIBEVENT) fork2.cpp -o fork2 -levent
fork3: 
	g++ -g $(INC_LIBEVENT) fork3.cpp -o fork3 -levent
client: 
	g++ -g client.cpp -o client 
clean:
	rm -rf server client mysqlbind mysqltest fork fork2 fork3 *.o *.a *.pb.cc *.pb.h
gencode:
	protoc -I=./ --cpp_out=./ sqlparam.proto
	protoc -I=./ --cpp_out=./ result.proto
	
libsqlparam: gencode
	g++ $(INC_PROTOBUF) $(LIB_PROTOBUF) -c sqlparam.pb.cc
	ar crv libsqlparam.a sqlparam.pb.o
libresult: gencode
	g++ $(INC_PROTOBUF) $(LIB_PROTOBUF) -c result.pb.cc
	ar crv libresult.a result.pb.o
mysqltest: libsqlparam libresult
	g++ mysqltest.cpp -o mysqltest $(COMMON_PATH)
	./mysqltest
mysqlbind: 
	g++ mysqlbind.cpp -o mysqlbind $(COMMON_PATH)
	./mysqlbind
test:
	g++ test.c -o test
run:
	echo 'ha'
