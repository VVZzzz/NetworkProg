#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 3000
#define SEND_DATA "helloworld"

int main(int argc, char **argv) {
  // 1. ----socket创建socket----
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cout << "create client socket error!" << std::endl;
    return -1;
  }

  // 2. ----connect连接服务器----
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  serveraddr.sin_port = htons(SERVER_PORT);
  if (connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) ==
      -1) {
    std::cout << "connect error!" << std::endl;
    return -1;
  }

  // connect之后再改变clientsocket为非阻塞的
  //一开始就创建为非阻塞的会改变connect行为
  //首先得到clientfd旧的标志位
  int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
  int newSocketFlag = oldSocketFlag | O_NONBLOCK;
  //修改clientfd标志位
  if (fcntl(clientfd, F_SETFL, newSocketFlag) == -1) {
    std::cout << "set clientfd nonblock error!" << std::endl;
    close(clientfd);
    return -1;
  }

  // 3. ----recv向服务器接收数据----
  int count = 0;
  while (true) {
      char recvbuf[32] = {0};
    int ret = recv(clientfd, recvbuf, 32,0);
    if (ret == -1) {
      //非阻塞socket中recv因没有收到数据会立即返回,置位EWOULDBLOCK
      if (errno == EWOULDBLOCK) {
        std::cout << "There is no data avaliable."
                  << std::endl;
        continue;
      } else if (errno == EINTR) {
        //被信号中断
        std::cout << "recv data interrupted by signal." << std::endl;
        continue;
      } else {
        std::cout << "recv data error!" << std::endl;
        break;
      }
    } else if (ret == 0) {
      //对方关闭连接
      std::cout << "server close connection!" << std::endl;
      break;
    } else {
      count++;
      std::cout << "recv data successfully! count: " << count << std::endl;
    }
  }

  // 5. ----关闭socket----
  close(clientfd);
  return 0;
}