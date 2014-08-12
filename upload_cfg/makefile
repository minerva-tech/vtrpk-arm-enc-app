CC = clang++
#CC = g++

#CFLAGS += -I../../../boost/boost_1_53_0 -I../ -std=c++0x -O2 -v
CFLAGS += -I../../../boost/boost_1_53_0 -I../ -std=c++0x -O2
LDFLAGS += -L../../../boost/boost_1_53_0/stage/lib -lboost_chrono -lboost_system -lboost_thread -lrt -pthread -static

all:
	$(CC) $(CFLAGS) ../comm.cpp ../proto.cpp main.cpp $(LDFLAGS)
