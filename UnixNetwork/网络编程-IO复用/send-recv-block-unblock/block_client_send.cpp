#include <arpa/inet.h>
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

  // 3. ----send向服务器发送数据----
  int count = 0;
  while (true) {
    int ret = send(clientfd, SEND_DATA, strlen(SEND_DATA), 0);
    if (ret != strlen(SEND_DATA)) {
      std::cout << "send error!" << std::endl;
      return -1;
    } else {
      count++;
      std::cout << "send data successfully! count: " << count << std::endl;
    }
  }

  // 5. ----关闭socket----
  close(clientfd);
  return 0;
}