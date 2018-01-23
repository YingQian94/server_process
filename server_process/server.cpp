#include "server.h"
#include <iostream>
#include <memory>
using namespace std;
const char DEFAULTPATH[30]="./tmpImg/";

int server::m_epollfd=-1;

void server::init(int epollfd,int sockfd){
    m_epollfd=epollfd;
    m_sockfd=sockfd;
}

void server::process()
{
    do_socket_read();
    k_means();
    do_socket_write();
}

void server::Error(int m_sockfd,int n)    //socket错误处理
{
    if(n==0)
    {
        printf("client close\n");
        close(m_sockfd);
        struct epoll_event ev;
        ev.events=0;
        ev.data.fd=m_sockfd;
        epoll_ctl(m_epollfd,EPOLL_CTL_DEL,m_sockfd,&ev);
    }
    else if(n<0){
        if(!(errno==EAGAIN || errno==EWOULDBLOCK))
        {
            printf("error \n");
            close(m_sockfd);
            struct epoll_event ev;
            ev.events=0;
            ev.data.fd=m_sockfd;
            epoll_ctl(m_epollfd,EPOLL_CTL_DEL,m_sockfd,&ev);
        }
    }
}

bool server::do_socket_read() //epoll中event描述符为EPOLLIN时需要处理socket读
{
    int n;
    char buff[MAXLINE];
    //file length 读取
    {
        long l;
        memset(buff,0,MAXLINE);
        n=readn(m_sockfd,buff,sizeof(long));
        memcpy(&l,buff,sizeof(long));
        l=(long)ntohl(l);
        if(n>0)
        {
            d.fileLen=l;
        }    
        else
            Error(m_sockfd,n);
    }
    //k 读取
    {
        int kRev;
        memset(buff,0,MAXLINE);
        n=readn(m_sockfd,buff,sizeof(int));
        memcpy(&kRev,buff,sizeof(int));
        kRev=(int)ntohl(kRev);
        if(n>0)
        {
            d.k=kRev;
        }    
        else
            Error(m_sockfd,n);
    }
    //imagename 读取
    {
        memset(buff,0,MAXLINE);
        n=readn(m_sockfd,buff,MAXLINE);
        if(n>0)
        {
            memcpy(d.imagename,buff,strlen(buff)+1);
        }
        else
            Error(m_sockfd,n);
        char ttmp[NAMELEN],printstr[NAMELEN];
        {
            strcpy(ttmp,d.imagename);
        }
        char *tmp;
        tmp=strrchr(ttmp,'/')+1;
        memcpy(printstr,DEFAULTPATH,strlen(DEFAULTPATH));
        struct sockaddr_in client;
        socklen_t addrlen;
        getpeername(m_sockfd,(struct sockaddr *)&client,&addrlen);
        char addr_c[20],port_c[10],m_sockfd_c[10];
        inet_ntop(AF_INET,&client.sin_addr.s_addr,addr_c,sizeof(addr_c));
        int port=ntohs(client.sin_port);
        sprintf(port_c,"%d",port);
        sprintf(m_sockfd_c,"%d",m_sockfd);
        memcpy(printstr+strlen(DEFAULTPATH),addr_c,strlen(addr_c));
        memcpy(printstr+strlen(DEFAULTPATH)+strlen(addr_c),port_c,strlen(port_c));
        memcpy(printstr+strlen(DEFAULTPATH)+strlen(addr_c)+strlen(port_c),m_sockfd_c,strlen(m_sockfd_c));
        memcpy(printstr+strlen(DEFAULTPATH)+strlen(addr_c)+strlen(port_c)+strlen(m_sockfd_c),tmp,strlen(tmp));
        printstr[strlen(DEFAULTPATH)+strlen(addr_c)+strlen(port_c)+strlen(m_sockfd_c)+strlen(tmp)]='\0';
        //printf("d.back().filename:%s\n",d.filename);
        sprintf(d.filename,"%s",printstr);
    }
    //文件读取
    {
        memset(buff,0,MAXLINE);
        char filename[NAMELEN];
        {
            strcpy(filename,d.filename);
        }
        FILE *fp=fopen(filename,"wb");
        if(fp==NULL)
        {
            printf("open failed\n");
            return false;
        }
        long fileLen,getLen=0;
        {
            fileLen=d.fileLen;
        }
        while(getLen<fileLen)
        {
            if((fileLen-getLen)>MAXLINE)
                n=readn(m_sockfd,buff,MAXLINE);
            else
                n=readn(m_sockfd,buff,fileLen-getLen);
            if(n>0)
            {
                fwrite(buff,n,1,fp);
                getLen+=n;
            }
            else if(n<0)
            {
                if(!(errno==EAGAIN || errno==EWOULDBLOCK))
                {
                    printf("error \n");
                    close(m_sockfd);
                    fclose(fp);
                    break;
                }
            }    
            else 
            {
                printf("client close\n");
                close(m_sockfd);
                fclose(fp);
                break;
            }
            if(getLen==fileLen)
            {
                fflush(fp);
                fclose(fp);
                return true;
            }
        }
        return false;
    }
}

void server::do_socket_write() //服务器发送图片
{
    char buff[MAXLINE];
    char outname[NAMELEN],filename[NAMELEN];
    strcpy(filename,d.filename);
    memcpy(outname,filename,strlen(filename)-4);
    memcpy(outname+strlen(filename)-4,"_mainColor.jpg",strlen("_mainColor.jpg"));
    outname[strlen(filename)-4+strlen("_mainColor.jpg")]='\0';
    //printf("record[%d].outname:%s\n",m_sockfd,outname);
    int n;
    memset(buff,0,MAXLINE);
    memcpy(buff,outname,strlen(outname)+1);
    n=writen(m_sockfd,buff,MAXLINE);       //发送 filename
    //printf("send filename finish:%d\n",n);
    if(n<=0)
        Error(m_sockfd,n);
    memset(buff,0,MAXLINE);
    //printf("start open file\n");
    FILE *fp=fopen(outname,"rb");
    if(fp==NULL) 
    {
        printf("filename:%s\n",outname);
        printf("filename is incorrect\n");
        return ;
    }
    // else
    //     printf("open file success\n");
    fseek(fp,0,SEEK_END);
    long len=ftell(fp),needSend=len;
    len=htonl(len);
    rewind(fp);
    memcpy(buff,&len,sizeof(long));
    int writeLen=writen(m_sockfd,buff,sizeof(long));   //发送文件长度
    assert(writeLen==sizeof(long));
    memset(buff,0,MAXLINE);
    while(needSend>MAXLINE)                         //发送文件
    {
        fread(buff,MAXLINE,1,fp);
        int sendLen=writen(m_sockfd,buff,MAXLINE);
        assert(sendLen==MAXLINE);
        memset(buff,0,MAXLINE);
        needSend-=MAXLINE;
    }
    if(needSend<=MAXLINE)
    {
        fread(buff,needSend,1,fp);
        int sendLen=writen(m_sockfd,buff,needSend);
        assert(sendLen==needSend);
    }
    fclose(fp);
    remove(outname);
    remove(filename);
    printf("finish send image\n");
    shutdown(m_sockfd,SHUT_RD);  //无法接收数据，receive buffer 被丢弃掉，可以发送数据
    struct epoll_event ev;
    ev.events=0;
    ev.data.fd=m_sockfd;
    epoll_ctl(m_epollfd,EPOLL_CTL_DEL,m_sockfd,&ev);
}

void server::k_means()
{
    Mat small,image;
    int k=d.k;
    char imagename[NAMELEN];
    strcpy(imagename,d.imagename);
    image=imread(imagename);
    int r=(int)image.rows/10,c=(int)image.cols/10;
    resize(image,small,Size(r,c),0,0,INTER_AREA);
    vector<vector<int>> k_color(k,vector<int>(3));  //中心点rgb值
    const int MAXSTEP=800;                          //最大迭代次数100
    int change=0;                                   //记录k个中心点是否收敛
    srand((unsigned)time(NULL));
    for(int i=0;i<k;i++)                            //初始化k个中心点
    {
        int x_pos,y_pos;
        x_pos=rand()%small.rows;
        y_pos=rand()%small.cols;
        k_color[i][0]=(int)small.at<Vec3b>(x_pos,y_pos)[0];
        k_color[i][1]=(int)small.at<Vec3b>(x_pos,y_pos)[1];
        k_color[i][2]=(int)small.at<Vec3b>(x_pos,y_pos)[2];
    }
    int step=0;
    while(step<MAXSTEP && change<k)
    {
        step++;
        change=0;
        vector<vector<pair<int,int>>> KGROUPS;      //用于存放k种聚类后颜色的坐标点构成
        KGROUPS.resize(k);
        for(int i=0;i<small.rows;i++)               //每个点加入距离最短的group
            for(int j=0;j<small.cols;j++)
            {
                vector<int> dis;
                for(int r=0;r<k;r++)
                    dis.push_back(abs((int)small.at<Vec3b>(i,j)[0]-k_color[r][0])+
                abs((int)small.at<Vec3b>(i,j)[1]-k_color[r][1])+
                abs((int)small.at<Vec3b>(i,j)[2]-k_color[r][2]));
                int min_index=-1,min_value=999999;
                for(int r=0;r<k;r++)
                    if(dis[r]<min_value)
                    {
                        min_value=dis[r];
                        min_index=r;
                    }
                KGROUPS[min_index].push_back({i,j});
            }
        for(int r=0;r<k;r++)                        //重新计算k个类的中心点
        {
            int B_color=0,G_color=0,R_color=0;
            if(KGROUPS[r].size()==0)                //某一类没有分配到与他最近的中心点，则随机初始化
            {
                k_color[r][0]=rand()%256;
                k_color[r][1]=rand()%256;
                k_color[r][2]=rand()%256;
            }
            else{
                for(unsigned int i=0;i<KGROUPS[r].size();i++)
                {
                    B_color+=(int)small.at<Vec3b>(KGROUPS[r][i].first,KGROUPS[r][i].second)[0];
                    G_color+=(int)small.at<Vec3b>(KGROUPS[r][i].first,KGROUPS[r][i].second)[1];
                    R_color+=(int)small.at<Vec3b>(KGROUPS[r][i].first,KGROUPS[r][i].second)[2];
                }
                B_color/=KGROUPS[r].size();
                G_color/=KGROUPS[r].size();
                R_color/=KGROUPS[r].size();
                if(B_color==k_color[r][0] && G_color==k_color[r][1] && R_color==k_color[r][2])
                    change+=1;
                k_color[r][0]=B_color;
                k_color[r][1]=G_color;
                k_color[r][2]=R_color;
            }
        }
    }
    char outname[NAMELEN],filename[NAMELEN];
    strcpy(filename,d.filename);
    memcpy(outname,filename,strlen(filename)-4);
    memcpy(outname+strlen(filename)-4,"_mainColor.jpg",strlen("_mainColor.jpg"));
    outname[strlen(filename)-4+strlen("_mainColor.jpg")]='\0';
    int writeStep=(int)small.rows/k;                  //输出主颜色到图片
    for(int r=0;r<k;r++)
    {
        int end=(r==k-1)?small.rows:writeStep*(r+1);
        for(int i=writeStep*r;i<end;i++)
            for(int j=0;j<small.cols;j++)
            {
                small.at<Vec3b>(i,j)[0]=k_color[r][0];
                small.at<Vec3b>(i,j)[1]=k_color[r][1];
                small.at<Vec3b>(i,j)[2]=k_color[r][2];
            }
    }
    // printf("k_means_outname:%s\n",outname);
    imwrite(outname,small);
    printf("k_means end\n");
    return ;
}
