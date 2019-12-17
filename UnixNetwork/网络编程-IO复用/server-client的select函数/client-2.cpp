
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // 1. 创建socket
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cout << "create clientfd error." << std::endl;
    return -1;
  }

  // 2.connect连接服务器
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serveraddr.sin_port = htons(3000);
  if (connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) ==
      -1) {
    std::cout << "connect server error." << std::endl;
    close(clientfd);
    return -1;
  }

  while (true) {
    fd_set readset;
    FD_ZERO(&readset);

    //将clientfd加入到待检测的可读事件中去
    FD_SET(clientfd, &readset);
    //timeval tm;
    //tm.tv_sec = 50;
    //tm.tv_usec = 0;
    int ret;

    //暂且只检测可读事件，不检测可写和异常事件
    ret = select(clientfd + 1, &readset, NULL, NULL, NULL);
    std::cout << "tm.tv_sec: " << tm.tv_sec << ", tm.tv_usec: " << tm.tv_usec
              << std::endl;
    if (ret == -1) {
      //除了被信号中断的情形，其他情况都是出错
      if (errno != EINTR) break;
    } else if (ret == 0) {
      // select函数超时
      std::cout << "no event in specific time interval" << std::endl;
      continue;
    } else {
      if (FD_ISSET(clientfd, &readset)) {
        //检测到可读事件
        char recvbuf[32];
        memset(recvbuf, 0, sizeof(recvbuf));
        //假设对端发数据的时候不超过31个字符。
        int n = recv(clientfd, recvbuf, 32, 0);
        if (n < 0) {
          //除了被信号中断的情形，其他情况都是出错
          if (errno != EINTR) break;
        } else if (n == 0) {
          //对端关闭了连接
          break;
        } else {
          std::cout << "recv data: " << recvbuf << std::endl;
        }
      } else {
        std::cout << "other socket event." << std::endl;
      }
    }
  }

  return 0;
}