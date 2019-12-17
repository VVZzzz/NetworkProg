/**
 * 利用基于升序链表的定时器处理非活动连接
 * 即在应用层实现KEEPALIVE机制
 * 使用统一事件源标志
 */
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include "sort-timer-list.h"

#define FD_LIMIT 65536
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_list timer_list;
static int epollfd = 0;

int setnonblock(int fd) {
  int old_flag = fcntl(fd, F_GETFL, 0);
  int new_flg = old_flag | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_flg);

  return old_flag;
}

void addfd(int epollfd, int fd) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblock(fd);
}

void sig_handler(int signo) {
  int save_errno = errno;
  int msg = signo;
  //将信号传递给管道写端
  send(pipefd[1], (char *)&msg, 1, 0);
  errno = save_errno;
}

void addsig(int signo) {
  struct sigaction action;
  bzero(&action, sizeof(action));
  action.sa_handler = sig_handler;
  action.sa_flags |= SA_RESTART;
  sigfillset(&action.sa_mask);
  sigaction(signo, &action, NULL);
}

//处理定时任务(实际即调用tick()),并重新设置定时器
void timer_handler() {
  timer_list.tick();
  //重新开始新的定时
  alarm(TIMESLOT);
}

//定时器回调函数,将非活动连接上的事件从epoll上删除,并关闭
void cb_func(client_data *usr_data) {
  if (!usr_data) return;
  epoll_ctl(epollfd, EPOLL_CTL_DEL, usr_data->sockfd, 0);
  close(usr_data->sockfd);
  std::cout << "close fd :" << usr_data->sockfd << std::endl;
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

  epoll_event events[MAX_EVENT_NUMBER];
  epollfd = epoll_create(5);
  addfd(epollfd, listenfd);

  //创建管道
  socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
  setnonblock(pipefd[1]);
  //监听管道读端上的事件
  addfd(epollfd, pipefd[0]);

  //设置信号处理函数
  addsig(SIGALRM);
  addsig(SIGTERM);
  bool stop_server = false;

  client_data *users = new client_data[FD_LIMIT];
  bool timeout = false;
  //开始定时,检测不活动的连接
  alarm(TIMESLOT);

  while (!stop_server) {
    int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    for (int i = 0; i < num; i++) {
      int sockfd = events[i].data.fd;
      if (sockfd == listenfd) {
        //有新连接到来
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
        if (connfd < 0) continue;
        //设置新到来客户的数据信息,并设置他的定时器
        addfd(epollfd, connfd);
        users[connfd].address = clientaddr;
        users[connfd].sockfd = connfd;
        util_timer *client_timer = new util_timer();
        client_timer->user_data = &users[connfd];
        client_timer->cb_func = cb_func;
        time_t cur = time(NULL);
        client_timer->expire = cur + 3 * TIMESLOT;
        users[connfd].timer = client_timer;
        //加入到定时器链表中
        timer_list.add_timer(client_timer);
      } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
        //有信号到来,则处理
        int sig;
        char signals[1024];
        //到来的信号存放在signals中
        int ret = recv(sockfd, signals, 1024, 0);
        if (ret == -1) {
          continue;
        } else if (ret == 0) {
          continue;
        } else {
          for (int i = 0; i < ret; i++) {
            switch (signals[i]) {
              case SIGALRM: {
                //由于定时事件优先级低,先处理IO事件
                timeout = true;
                break;
              }
              case SIGTERM: {
                //关闭服务器
                stop_server = true;
              }
            }
          }
        }
      } else if (events[i].events & EPOLLIN) {
        //客户发来数据,先收
        memset(users[sockfd].buf, '\0', sizeof(users[sockfd].buf));
        int ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
        std::cout << "recv data from clientfd " << sockfd << " : "
                  << users[sockfd].buf << std::endl;
        util_timer *timer = users[sockfd].timer;

        if (ret < 0) {
          if (errno != EAGAIN) {
            //读错误,则删除在epollfd中删除定时器事件
            //并在链表中删除定时器
            cb_func(&users[sockfd]);
            if (timer) timer_list.del_timer(timer);
          }
        } else if (ret == 0) {
          //对方关闭连接
          cb_func(&users[sockfd]);
          if (timer) timer_list.del_timer(timer);
        } else {
          //成功读到数据,则调整这个定时器的超时时间(将它在链表中往后排)
          if (timer) {
            time_t cur = time(NULL);
            timer->expire = cur + 3 * TIMESLOT;
            std::cout << "adjust timer once" << std::endl;
            timer_list.adjust_timer(timer);
          }
        }
      } else {
        // other things happend;
      }
    }
    //最后再处理定时器事件,IO优先级更高.
    //这样的话会造成定时任务不准确,因为比如第一个客户是超时了,但不执行定时任务,
    //而是等遍历所有客户之后,统一执行
    if (timeout) {
      //这里的定时任务即cb_func(简单的作为例子)
      timer_handler();
      timeout = false;
    }
  }
  close(listenfd);
  close(pipefd[0]);
  close(pipefd[1]);
  delete[] users;
  return 0;
}