# server_process
进程池版图像服务器

 ![image](https://github.com/YingQian94/server_process/blob/master/server_process/model.png)

1.主进程负责监听listenfd上的新连接

2.主进程监听到新连接请求使用round robin选择一个子进程通知，子进程完成一个新请求的read,compute,write操作

3.主进程和子进程之前的通信方式选择管道，因为管道有文件描述符，可以使用epoll的IO多路复用功能

4.主进程以及每个子进程有自己的epollfd
  
  主进程的epollfd,负责监听listenfd,以及信号事件（信号事件转化为一个全局管道的读写处理，信号处理函数向管道中写，主进程和子进程从管道中读）
  
  子进程的epollfd,负责监听与父进程的管道读端以及信号事件
