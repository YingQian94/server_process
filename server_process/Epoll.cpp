#include "Epoll.h"

void add_event(int fd,int epollfd)
{
    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLET;
    ev.data.fd=fd;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}

void delete_event(int fd,int epollfd)
{
    struct epoll_event ev;
    ev.events=EPOLLIN|EPOLLET;
    ev.data.fd=fd;
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
    close(fd);
}
