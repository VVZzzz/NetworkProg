#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

int main(int arg, char **argv) {
  // 1.创建listensocket
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "create listen socket error." << std::endl;
    return -1;
  }

  // 2. 初始化服务器地址
  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  bindaddr.sin_port = htons(3000);
  if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1) {
    std::cout << "bind socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 3. 启动侦听
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "listen socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 4. 接收连接
  while (true) {
    struct sockaddr_in clientaddr;
    socklen_t clientaddrlen = sizeof(clientaddr);
    int clientfd =
        accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
    if (clientfd == -1) {
      //出错
      break;
    }

    //不调用send recv
    std::cout<<"accept a client connection."<<std::endl;
  }

  //5. 关闭连接
  close(listenfd);
  return 0;
}