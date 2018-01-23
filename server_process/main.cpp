#include "ProcessPool.h"
#include <vector>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <signal.h>

int main()
{
    //pthread_mutex_init(&mapMutex,NULL);
    int sockfd;
    if((sockfd=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        perror("socket failed!\n");
    }    
    struct sockaddr_in my_addr;
    my_addr.sin_family=AF_INET;
    my_addr.sin_port=htons(PORT);
    my_addr.sin_addr.s_addr=INADDR_ANY;

    //设置监听套接字为SO_REUSEADDR,服务器程序停止后想立即重启，而新套接字依旧使用同一端口
    //但必须意识到，此时任何非期望数据到达，都可能导致服务程序反应混乱，需要慎用
    int nOptval=1;                                                                          
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const void*)&nOptval,sizeof(int))<0)      
    {
        perror("set SO_REUSEADDR error\n");
    }
    if(bind(sockfd,(struct sockaddr*)&my_addr,sizeof(my_addr))==-1)
    {
        perror("bind failed!");
    }    

    //设置套接字存活属性
    int keepalive=1;
    if(setsockopt(sockfd,SOL_SOCKET,SO_KEEPALIVE,(void *)&keepalive,sizeof(keepalive))<0)
    {
        perror("set SO_KEEPALIVE error");
    }
    //设置keepalive空闲间隔时间，默认2h
    int keepalive_time=1000; 
    if(setsockopt(sockfd,IPPROTO_TCP,TCP_KEEPIDLE,(void *)&keepalive_time,sizeof(keepalive_time))<0)
    {
        perror("set KEEPIDLE error");
    }
    //设置探测消息发送频率，默认75s
    int keepalive_intvl=30;
    if(setsockopt(sockfd,IPPROTO_TCP,TCP_KEEPINTVL,(void *)&keepalive_intvl,sizeof(keepalive_intvl))<0)
    {
        perror("set KEEPINTVL error");
    }
    //设置发送探测消息次数，默认9
    int keepalive_probes=3;
    if(setsockopt(sockfd,IPPROTO_TCP,TCP_KEEPCNT,(void *)&keepalive_probes,sizeof(keepalive_probes))<0)
    {
        perror("set KEEPCNT error");
    }
    //设置tcp nodelay，不会将小包进行拼接成大包再进行发送，而是直接将小包发送出去
    int tcpnodelay=1;
    if(setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY,(void *)&tcpnodelay,sizeof(tcpnodelay))<0)
    {
        perror("set TCP_NODELAY error");
    }

    if(listen(sockfd,CONNECTNUM)==-1)
    {
        perror("listen failed!");
    }    

    setnonblock(sockfd);
    ProcessPool pool(sockfd);
    pool.run();     //子进程和父进程都会执行run函数，只是子进程和父进程具体执行run_child还是run_parent
    close(sockfd);
    return 0;
}

