#ifndef _SERVER_H
#define _SERVER_H

#include "Data.h"
#include <map>
#include <vector>
#include "efun.h"
#include "Epoll.h" 

class server{
public:
    server(){}
    ~server(){}
    void init(int epollfd,int sockfd);
    void process();
private:
    static int m_epollfd;
    int m_sockfd;
    Data d;
    void Error(int m_sockfd,int n);
    bool do_socket_read();
    void do_socket_write();
    void k_means();
};

#endif
