CC = clang++
#CC = g++

CFLAGS += -I../../../boost/boost_1_53_0 -std=c++0x -O2 -v
LDFLAGS += -L../../../boost/boost_1_53_0/stage/lib -lboost_system -lboost_thread -lrt -pthread -static

all:
	$(CC) $(CFLAGS) main.cpp $(LDFLAGS)
