/**
 * reactor模式 - epoll模型服务器demo
 */
#include <arpa/inet.h>  //for htonl() and htons()
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>  //for signal()
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iomanip>  //for std::setw()/setfill()
#include <iostream>
#include <list>
#include <sstream>

#define WORKER_THREAD_NUM 5
/**
 * 全局变量区
 */
int g_listenfd = 0;
int g_epollfd = 0;

bool create_server_listener(const char*ip,int port){
    //非阻塞监听socket
    g_listenfd = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
    int on = 1;
    //注意这里为何要设置REUSEADDR/PORT
    /**
     * 作为服务器端程序最好对侦听socket调用setsocketopt()设置SO_REUSEADDR和SO_REUSEPORT两个标志，
     * 因为服务程序有时候会需要重启（比如调试的时候就会不断重启），如果不设置这两个标志的话，绑定端口时就会调用失败。
     * 因为一个端口使用后，即使不再使用，因为四次挥手该端口处于TIME_WAIT状态，有大约2min的MSL（Maximum Segment Lifetime，最大存活期）。
     * 这2min内，该端口是不能被重复使用的。你的服务器程序上次使用了这个端口号，接着重启，因为这个缘故，你再次绑定这个端口就会失败（bind函数调用失败）。
     * 要不你就每次重启时需要等待2min后再试（这在频繁重启程序调试是难以接收的），或者设置这种SO_REUSEADDR和SO_REUSEPORT立即回收端口使用。
     */
    setsockopt(g_listenfd,SOL_SOCKET,SO_REUSEADDR,(char *)&on,sizeof(on));
    setsockopt(g_listenfd,SOL_SOCKET,SO_REUSEPORT,(char *)&on,sizeof(on));

    struct sockaddr_in servaddr;
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    if(bind(g_listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1) return false;
    if(listen(g_listenfd,SOMAXCONN) == -1) return false;
    epoll_create()
}


int main(int argc, char *argv[]) {
  int ch;
  int port = 0;
  bool bdaemon = false;  //以daemon方式运行
  while ((ch = getopt(argc, argv, "p:d")) != -1) {
    switch (ch) {
      case 'd':
        bdaemon = true;
        break;
      case 'p':
        port = atol(optarg);
        break;
    }
  }

  //if(bdaemon) daemon_run();
  if(port == 0) port = 12345;
}