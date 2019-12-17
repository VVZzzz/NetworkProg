/*
一般来说,在信号处理程序中应尽快执行完毕.因为在信号处理程序中
会阻塞该信号,所以大多数处理程序是这样做的:
在处理函数中将"信号"通过管道传递给主循环中,处理函数在管道写端写数据,
主循环从读端读数据,主循环何时知道有数据到来? 用IO复用技术,
这样信号事件也变成了IO事件,即统一为I/O事件了
Libevent I/O xinted等都这样统一为I/O事件
*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

//设置非阻塞fd
int setnonblock(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

//将fd的事件添加到内核事件表epollfd中
void addfd(int epollfd, int fd) {
  epoll_event event;
  event.data.fd = fd;
  //设置可读事件,边缘触发
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  //设置fd为非阻塞
  setnonblock(fd);
}

void sig_handler(int signo) {
  //保留原来的errno,在函数最后恢复,保证函数的可重入性
  //(可重入就是解决了函数使用全局,静态变量引起的竞争问题)
  int save_errno = errno;
  int msg = signo;
  //注意这里的用法,直接将int的msg当作一个char数组用
  //将fd写入管道
  send(pipefd[1], (char *)&msg, 1, 0);
  errno = save_errno;
}

void addsig(int signo) {
  struct sigaction sa;
  bzero(&sa, sizeof(sa));
  sa.sa_handler = sig_handler;
  sa.sa_flags |= SA_RESTART;  //从信号处理程序返回后,重启系统调用
  sigfillset(&sa.sa_mask);
  sigaction(signo, &sa, NULL);
}

int main(int argc, char **argv) {
  if (argc <= 2) {
    std::cout << "usage: ipaddr  portnumber" << std::endl;
    return 1;
  }
  const char *addr = argv[1];
  int port = atoi(argv[2]);

  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  inet_pton(AF_INET, addr, &serveraddr.sin_addr);

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    return -1;
  }

  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    return -1;
  }

  if (listen(listenfd, 5) < 0) {
    return -1;
  }

  //申请内核事件表,大小为5
  int epollfd = epoll_create(5);
  epoll_event events[MAX_EVENT_NUMBER];
  if (epollfd < 0) return -1;
  addfd(epollfd, listenfd);

  //用socketpair创建一个全双工的流管道
  // pipefd[0]注册为读事件
  if (socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) < 0) return -1;
  setnonblock(pipefd[1]);
  addfd(epollfd, pipefd[0]);

  //设置信号的处理函数
  addsig(SIGHUP);
  addsig(SIGCHLD);
  addsig(SIGTERM);
  addsig(SIGINT);
  bool stop_server = false;

  while (true) {
    int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    if (number < 0 && (errno != EINTR)) {
      break;
    }
    for (int i = 0; i < number; i++) {
      int sockfd = events[i].data.fd;
      if (sockfd == listenfd) {
        //有新连接到来
        struct sockaddr_in clientaddr;
        socklen_t len = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &len);
        //将客户fd添加到事件表中
        addfd(epollfd, connfd);
      } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
        //如果是管道有数据可读
        int sig;
        char signals[1024];
        int ret = recv(pipefd[0], signals, sizeof(signals), 0);
        if (ret == -1)
          continue;
        else if (ret == 0)
          continue;
        else {
          //由于信号值可以用一个char型表示(1个字节就够),signals存的就是发来的信号
          //即在主循环中处理信号
          for (int i = 0; i < ret; i++) {
            switch (signals[i]) {
              case SIGCHLD:
                break;
              case SIGHUP:
                continue;
              case SIGTERM:
              case SIGINT:
                stop_server = true;
                break;
            }
          }
        }
      } else {
        // other things happend;
      }
    }
  }
  //关闭fd
  close(listenfd);
  close(pipefd[0]);
  close(pipefd[1]);
  return 0;
}