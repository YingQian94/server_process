#ifndef _PROCESSPOOL_H
#define _PROCESSPOOL_H

#include "efun.h"
#include "Epoll.h"
#include "server.h"

/*************************************/
/*描述一个子进程类，m_pid是子进程的PID，m_pipefd是父进程
和子进程通信用的管道
*/
/*************************************/
class Process{
public:
    Process():m_pid(-1){}
    pid_t m_pid;
    int m_pipefd[2];
};

class ProcessPool{
public:
    ProcessPool(int listenfd,int process_number=8);
    ~ProcessPool(){
        delete []m_sub_process;
    }
    void run();
private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();

    static const int MAX_PROCESS_NUMBER=16; //进程池允许的最大子进程数量
    static const int USER_PER_PROCESS=5000;//每个子进程最多能处理的客户数量
    static const int MAX_EVENT_NUMBER=1000;//epoll最多能处理的事件数
    int m_process_number;                   //进程池中的进程总数
    int m_idx;                              //子进程在池中的序号，从0开始，-1为父进程
    int m_epollfd;                          //每个进程都有自己的epoll
    int m_listenfd;                         //所有进程拥有相同的listenfd
    int m_stop;                             //子进程通过m_stop来决定是否停止运行
    Process *m_sub_process;                 //保存所有子进程的描述信息
};

#endif