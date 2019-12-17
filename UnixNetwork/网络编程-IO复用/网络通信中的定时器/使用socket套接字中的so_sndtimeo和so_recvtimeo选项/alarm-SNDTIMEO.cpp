/*
使用SO_SNDTIMEO标志,对connect进行超时设置
其中connect超时将errno设置为EINPROGRESS
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

int connect_timeo(int fd, const struct sockaddr* addr, socklen_t addrlen,
                  int nsec) {
  struct timeval timeout;
  timeout.tv_sec = nsec;
  timeout.tv_usec = 0;
  //首先设置socket超时选项
  int ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  if (ret == -1) {
    std::cout << "setsockopt error!" << std::endl;
    return -1;
  }
  ret = connect(fd, addr, addrlen);
  if (ret < 0) {
    if (errno == EINPROGRESS) {
      //说明此时connect已经超时
      std::cout << "connect timeout!" << std::endl;
      return -1;
    }
    std::cout << "connect other error! : " << errno << std::endl;
    return -1;  //其他错误
  }

  return ret;
}

int main(int argc, char** argv) {
  if (argc <= 2) {
    std::cout << "usage: ipaddr  portnumber" << std::endl;
    return 1;
  }
  const char* addr = argv[1];
  int port = atoi(argv[2]);

  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  inet_pton(AF_INET, addr, &serveraddr.sin_addr);

  int clientfd = socket(AF_INET, SOCK_STREAM, 0);

  int ret = connect_timeo(clientfd, (struct sockaddr*)&serveraddr,
                          sizeof(serveraddr), 5);
  if (ret < 0)
    std::cout << "connect error!" << std::endl;
  else
    std::cout << "connect success." << std::endl;
  close(clientfd);
  return 0;
}