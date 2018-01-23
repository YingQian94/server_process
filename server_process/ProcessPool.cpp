#include "ProcessPool.h"

static int sig_pipefd[2];                   //用于实现统一事件源，处理信号的管道

static void sig_handler(int sig){
    int save_errno=errno;
    int msg=sig;
    send(sig_pipefd[1],(char *)&msg,1,0);
    errno=save_errno;
}

ProcessPool::ProcessPool(int listenfd,int process_number):
    m_idx(-1),m_listenfd(listenfd),m_process_number(process_number),m_stop(false)
{
    assert((process_number>0) && (process_number<=MAX_PROCESS_NUMBER));

    m_sub_process=new Process[m_process_number];
    assert(m_sub_process);

    for(int i=0;i<m_process_number;++i)        //创建process_number个子进程，并建立他们和父进程之间的管道
    {
        int ret=socketpair(AF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        assert(ret==0);
        pid_t pid;
        if((pid=fork())<0)
        {
            perror("fork failed");
            break;
        }
        else if(pid>0)            //父进程
        {
            m_sub_process[i].m_pid=pid;
            //printf("i:%d,m_sub_process[i].m_pid:%d\n",i,pid);
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else{                                   //子进程
            close(m_sub_process[i].m_pipefd[0]);
            m_idx=i;
            //run_child();
            //exit(0);
            break;
        }
    }
    //run_parent();
}

void ProcessPool::run()
{
    if(m_idx!=-1)
    {
        run_child();
        return;
    }
    run_parent();
}

/*统一事件源*/
void ProcessPool::setup_sig_pipe(){
    m_epollfd=epoll_create(10);
    assert(m_epollfd!=-1);

    int ret=socketpair(AF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret!=-1);
    setnonblock(sig_pipefd[1]);     //非阻塞写入
    add_event(sig_pipefd[0],m_epollfd);

    Signal(SIGCHLD,sig_handler);
    Signal(SIGTERM,sig_handler);
    Signal(SIGINT,sig_handler);
    Signal(SIGPIPE,SIG_IGN);
    //Signal(SIGTTIN,SIG_IGN);
}

void ProcessPool::run_child(){
    setup_sig_pipe();
    int pipefd=m_sub_process[m_idx].m_pipefd[1];//每个子进程通过进程池中的序号值m_idx找到与父进程通信的管道
    add_event(pipefd,m_epollfd);
    struct epoll_event events[MAX_EVENT_NUMBER];

    server *users=new server[USER_PER_PROCESS];
    assert(users);
    int number=0;
    int ret=-1;
    while(!m_stop)
    {
        number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            perror("epoll failure\n");
            break;
        }
        for(int i=0;i<number;++i)
        {
            int sockfd=events[i].data.fd;
            if((sockfd==pipefd) && (events[i].events & EPOLLIN))
            {
                int client=0;
                //从父子进程之间的管道读取数据，并将结果保存在client中，如果读取成果表示有新用户到来
                ret=recv(sockfd,(char *)&client,sizeof(client),0);
                if(((ret<0) &&  (errno!=EAGAIN || errno!=EWOULDBLOCK)) || ret==0)
                {
                    continue;
                } 
                else{
                    struct sockaddr_in clientaddr;
                    socklen_t clientaddrlen=sizeof(clientaddr);
                    int connfd=accept(m_listenfd,(struct sockaddr*)&clientaddr,&clientaddrlen);
                    if(connfd<0)
                    {
                        perror("accept error\n");
                        continue;
                    }
                    setnonblock(connfd);
                    add_event(connfd,m_epollfd);
                    users[connfd].init(m_epollfd,connfd);
                }
            }
            else if((sockfd==sig_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                char signals[1024];
                ret=recv(sig_pipefd[0],signals,sizeof(signals),0);
                if(ret<=0)
                {
                    continue;
                }
                else {
                    for(int i=0;i<ret;++i)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid=waitpid(-1,&stat,WNOHANG))>0);
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                m_stop=true;
                                break;
                            }
                            default: break;
                        }
                    }
                }
            }
            //如果是其他可读数据，那么必然是客户请求到来，调用逻辑对象的process处理
            else if(events[i].events & EPOLLIN) 
            {
                users[sockfd].process();
            }
        }
    }
    delete []users;
    users=NULL; //防止出现悬垂指针
    close(pipefd);
    close(m_epollfd);
}

void ProcessPool::run_parent(){
    setup_sig_pipe();

    add_event(m_listenfd,m_epollfd);    //listenfd一定要开启EPOLLET，否则会重复触发同一个connfd
    struct epoll_event events[MAX_EVENT_NUMBER]; 

    int sub_process_counter=0;
    int new_conn=1;
    int number=0;
    int ret=-1;

    while(!m_stop)
    {
        number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0) && (errno!=EINTR))
        {
            perror("epoll failure\n");
            break;
        }
        for(int i=0;i<number;++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd && (events[i].events & EPOLLIN))
            {
                //如果有新连接到来，就采用Round Robin方式将其分配给一个子进程处理
                int j=sub_process_counter;
                do{
                    if(m_sub_process[j].m_pid!=-1)
                    {
                        break;
                    }
                    j=(j+1)%(m_process_number);
                }while(j!=sub_process_counter);

                if(m_sub_process[j].m_pid== -1)
                {
                    m_stop=true;
                    break;
                }
                sub_process_counter=(j+1)%m_process_number;
                send(m_sub_process[j].m_pipefd[0],(char *)&new_conn,sizeof(new_conn),0);
                printf("send request to child %d\n",j);
            }
            else if((sockfd==sig_pipefd[0])&&(events[i].events & EPOLLIN))
            {
                char signals[1024];
                ret=recv(sig_pipefd[0],signals,sizeof(signals),0);
                if(ret<=0)
                    continue;
                else
                {
                    for(int i=0;i<ret;++i)
                    {
                        switch(signals[i])
                        {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid=waitpid(-1,&stat,WNOHANG))>0)
                                {
                                    for(int j=0;j<m_process_number;++j)
                                    {
                                        //printf("j:%d,m_sub_process[i].m_pid:%d,pid:%d\n",j,m_sub_process[j].m_pid,pid);
                                        if(m_sub_process[j].m_pid==pid)
                                        {
                                            printf("child %d join\n",j);
                                            close(m_sub_process[j].m_pipefd[0]);
                                            m_sub_process[j].m_pid=-1;
                                        }
                                    }
                                }
                                //如果所有子进程都已经退出了，则父进程退出
                                m_stop=true;
                                for(int j=0;j<m_process_number;++j)
                                {
                                    if(m_sub_process[j].m_pid!=-1)
                                        m_stop=false;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                //如果父进程收到终止信号，那么就杀死所有子进程，并等待他们全部结束
                                printf("kill all the child now \n");
                                for(int j=0;j<m_process_number;++j)
                                {
                                    pid_t pid=m_sub_process[i].m_pid;
                                    if(pid!=-1)
                                        kill(pid,SIGTERM);
                                }
                                break;
                            }
                            default:break;
                        }
                    }
                }
            }
        }
    }
    close(m_epollfd);
}