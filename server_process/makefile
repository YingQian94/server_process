LD=`pkg-config --cflags --libs opencv`
FLAGS=-g -lpthread -Wall
G=g++ -std=c++11

all:client server 
client:client.h client.cpp efun.h efun.cpp
	$(G) $(FLAGS) -o client client.cpp efun.cpp efun.h 
server:main.cpp server.h server.cpp efun.h efun.cpp Epoll.h Epoll.cpp ProcessPool.h ProcessPool.cpp Data.h 
	$(G) $(FLAGS) -o server main.cpp server.h server.cpp efun.h efun.cpp Epoll.h Epoll.cpp ProcessPool.h ProcessPool.cpp Data.h $(LD)
clean:
	rm client server
