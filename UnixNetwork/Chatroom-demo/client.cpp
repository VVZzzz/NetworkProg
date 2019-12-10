/*
简易聊天室的client程序.
关键点是要将标准输入fd和socketfd进行IO复用
此处使用poll模型
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

int main(int argc, char **argv) {
  if (argc <= 2) {
    std::cout << "usage : " << argv[0] << "serveraddress port" << std::endl;
    return -1;
  }
  const char *ipaddr = argv[1];
  int port = atoi(argv[2]);

  // 1.connect server
  struct sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = PF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, ipaddr, &server_addr.sin_addr);

  // 2.clientsocket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    std::cout << "socket error!" << std::endl;
    return -1;
  }
  // 3.connect
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    std::cout << "connect error!" << std::endl;
    close(sockfd);
    return -1;
  }
  // 4.poll
  //注册标准输入fd(0)和socketfd可读事件
  pollfd fds[2];
  fds[0].fd = 0;
  fds[0].events = POLLIN;
  fds[0].revents = 0;
  fds[1].fd = sockfd;
  fds[1].events =
      POLLIN | POLLRDHUP;  // POLLRDHUP(linux2.6之后)是当对端关闭时,会检测到
  fds[1].revents = 0;

  char read_buf[1024];
  int pipefd[2];  // fd[0]读端,fd[1]写端
  int ret = pipe(pipefd);
  if (ret == -1) {
    std::cout << "pipe error!" << std::endl;
    close(sockfd);
    return -1;
  }

  while (true) {
    ret = poll(fds, 2, -1);
    if (ret < 0) {
      std::cout << "poll error!" << std::endl;
      break;
    }

    if (fds[1].revents & POLLRDHUP) {
      //服务器端关闭连接
      std::cout << "server close the connection!" << std::endl;
      break;
    } else if (fds[1].revents & POLLIN) {
      // socket接收到数据
      memset(read_buf, '\0', 1024);
      recv(fds[1].fd, read_buf, 1023, 0);
      std::cout << read_buf << std::endl;
    }

    if (fds[0].revents & POLLIN) {
      //标准输入fd有数据到来,将数据直接拷贝到sockfd上(零拷贝)
      //此处可以观察pipe和splice用法
      //数据流动方向即
      //即 sockfd <--- pipefd[0] <===pipe=== pipefd[1] <--- 0
      ret = splice(0, NULL, pipefd[1], NULL, 32768,
                   SPLICE_F_MORE | SPLICE_F_MOVE);
      ret = splice(pipefd[0], NULL, sockfd, NULL, 32768,
                   SPLICE_F_MORE | SPLICE_F_MOVE);
    }
  }
  close(sockfd);
  return 0;
}