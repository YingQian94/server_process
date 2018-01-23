#ifndef _EPOLL_H
#define _EPOLL_H

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>


void add_event(int fd,int epollfd);
void delete_event(int fd,int epollfd);

#endif