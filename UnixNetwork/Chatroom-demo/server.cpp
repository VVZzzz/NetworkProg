/*
简易聊天室的server程序.
此处使用poll模型管理listenfd和clientfd
为了时间效率,牺牲空间效率
*/
#define _GNU_SOURCE 1  //为了使用POLLRDHUP
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

#define FD_LIMIT 65536  // fd数量限制 , 即一台主机上确实最多这么多fd
#define USER_LIMIT 5  //最大用户数量限制

//客户端的信息
struct client_data {
  sockaddr_in address;
  char *writebuf = nullptr;  //要发送的数据
  char buf[1024];            //接收缓冲区
};

//设置为非阻塞fd
int set_nonblock(int fd) {
  int old_fd_flag = fcntl(fd, F_GETFL, 0);
  int new_fd_flag = old_fd_flag | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_fd_flag);
  return old_fd_flag;
}

int main(int argc, char **argv) {
  if (argc <= 2) {
    std::cout << "usage : " << argv[0] << "serveraddress port" << std::endl;
    return -1;
  }
  const char *ipaddr = argv[1];
  int port = atoi(argv[2]);

  // 1.listenfd
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "listen socket error!" << std::endl;
    return -1;
  }

  //设置重用
  int on = 1;
  setsockopt(listenfd,SOL_SOCKET,SO_REUSEPORT,&on,sizeof(on));
  setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

  // 2.bind
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  inet_pton(AF_INET, ipaddr, &serveraddr.sin_addr);
  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) ==
      -1) {
    std::cout << "bind error!" << std::endl;
    close(listenfd);
    return -1;
  }
  // 3.listen
  if (listen(listenfd, 5) == -1) {
    std::cout << "listen error!" << std::endl;
    close(listenfd);
    return -1;
  }

  // 4.poll,采用nonblock-fd
  //由于一台主机最多同时有65536个fd,那么我们建立一个client_data users[65536]
  //这样就直接把fd和客户关联起来了,但我们用poll设定一次就处理5个用户.
  client_data *users = new client_data[FD_LIMIT];
  //+1是包括listenfd
  pollfd pollfds[USER_LIMIT + 1];
  pollfds[0].fd = listenfd;
  pollfds[0].events = POLLIN | POLLERR;
  for (int i = 1; i < USER_LIMIT + 1; i++) {
    pollfds[i].fd = -1;
    pollfds[i].events = 0;
  }
  int user_counter = 0;  //统计当前有几个用户

  while (true) {
    int ret = poll(pollfds, USER_LIMIT + 1, -1);
    if (ret < 0) {
      std::cout << "poll error!" << std::endl;
      break;
    }
    for (int i = 0; i < user_counter + 1; i++) {
      if (pollfds[i].fd == listenfd && (pollfds[i].revents & POLLIN)) {
        //有新连接到来
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int sockfd =
            accept(listenfd, (struct sockaddr *)&client_addr, &addrlen);
        if (sockfd == -1) {
          std::cout << "accept error!" << std::endl;
          continue;
        }
        //如果此时有太多客户连接到服务器上,就断开这个新客户
        if (user_counter >= USER_LIMIT) {
          const char *msg = "too many clients!";
          send(sockfd, msg, strlen(msg), 0);
          close(sockfd);
          continue;
        }
        //不多,就添加这个客户
        ++user_counter;
        //设置非阻塞socket
        set_nonblock(sockfd);
        pollfds[user_counter].fd = sockfd;
        pollfds[user_counter].events = POLLIN | POLLRDHUP | POLLOUT;
        //添加用户信息
        users[sockfd].address = client_addr;
        std::cout << "new client arrive , fd: " << sockfd << std::endl;
      } else if (pollfds[i].revents & POLLERR) {
        //该fd上有错,令其continue
        std::cout << "error on fd: " << pollfds[i].fd << std::endl;
        char errmsg[100];
        memset(errmsg, '\0', 100);
        socklen_t length = sizeof(errmsg);
        if (getsockopt(pollfds[i].fd, SOL_SOCKET, SO_ERROR, &errmsg, &length) <
            0) {
          std::cout << "getsockopt error!" << std::endl;
        }
        std::cout << "error on fd: " << pollfds[i].fd << " is " << errmsg
                  << std::endl;
      } else if (pollfds[i].revents & POLLIN) {
        //普通fd上有数据到来
        int connfd = pollfds[i].fd;
        //清空该fd上的接收缓冲区
        memset(users[connfd].buf, '\0', 1024);
        //非阻塞的recv
        //回顾非阻塞的recv,应该是循环读,直到发生错误.(这里没用这样设置)
        int ret = recv(connfd, users[connfd].buf, 1024, 0);
        if (ret < 0) {
          if (errno != EAGAIN) {
            //读操作有错,则断开这个连接
            close(connfd);
            users[connfd] = users[pollfds[user_counter].fd];
            i--;
            user_counter--;
          }
        } else if (ret == 0) {
          //std::cout << "client disconnect!" << std::endl;
        } else {
          //读到数据,将数据转发给其他客户
          for (int j = 1; j <= user_counter; j++) {
            if (pollfds[j].fd == connfd) {
              continue;
            }
            //书上是这样写的应该有错
            // pollfds[j].events |= ~POLLIN;
            pollfds[j].events &= ~POLLIN;
            //读完数据后,注册可写事件
            pollfds[j].events |= POLLOUT;
            users[pollfds[j].fd].writebuf = users[connfd].buf;
          }
        }
      } else if (pollfds[i].revents & POLLRDHUP) {
        //如果客户端关闭连接,服务器这端也关闭连接
        int connfd = pollfds[i].fd;
        std::cout << "client: " << connfd << "left!" << std::endl;
        close(connfd);
        users[connfd] = users[pollfds[user_counter].fd];
        --i;
        --user_counter;
      } else if (pollfds[i].revents & POLLOUT) {
        //如果此时fd可写,即client端从stdin>sockfd
        int connfd = pollfds[i].fd;
        if (users[connfd].writebuf == nullptr) {
          continue;
        }
        int ret = send(connfd, users[connfd].writebuf,
                       strlen(users[connfd].writebuf), 0);
        //写完数据后,重新注册可读事件
        pollfds[i].events &= ~POLLOUT;
        pollfds[i].events |= POLLIN;
      }
    }
  }
  delete [] users;
  close(listenfd);
  return 0;
}