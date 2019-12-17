/**/
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // 1.clientfd
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cout << "create clientfd error!" << std::endl;
    return -1;
  }
  // 2.bind,可省略
  // 3.connect 注意此处阻塞与非阻塞的socket行为不同,此时的socket
  //还是阻塞的.

  //将socket设置为非阻塞
  int old_socket_flag = fcntl(clientfd, F_GETFL, 0);
  int new_socket_flag = old_socket_flag | O_NONBLOCK;
  if (fcntl(clientfd, F_SETFL, new_socket_flag) == -1) {
    close(clientfd);
    std::cout << "set nonblock fd error!" << std::endl;
    return -1;
  }

  struct sockaddr_in serveraddr;
  serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(3000);
  /*以下是阻塞socket的connect行为,即它会等待一个RTT时间,直到收到服务器发来的SYN+ACK*/
  /*
  if (connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) ==
      -1) {
          std::cout<<"connect error!"<<std::endl;
          return -1;
  }
  */
  /* 以下是非阻塞socket的connect行为,它会立即返回 */
  while (true) {
    int ret =
        connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    //返回0,说明客户端和服务器在同一主机上,连接被立即建立.
    if (ret == 0) {
      std::cout << "connect server success(the same host)!" << std::endl;
      break;
    } else if (ret == -1) {
      //发起connect之后,被信号打断
      if (errno == EINTR) {
        //按照UNP,此时不能够再次调用connect去等待未完成的连接
        //而是用select检测是否可写
        // continue(X)
        break;
      } else if (errno == EINPROGRESS) {
        //连接已发起,等待完成
        //此处可do otherthings;
        break;
      } else {
        //出错
        close(clientfd);
        break;
      }
    }
  }

  fd_set writeset;
  FD_ZERO(&writeset);
  FD_SET(clientfd, &writeset);
  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  int ret;
  if ((ret = select(clientfd + 1, nullptr, &writeset, nullptr, &tv)) ==
      0) {
    // select返回0,select超时出错
    close(clientfd);
    errno = ETIMEDOUT;
    std::cout << "select timeout!" << std::endl;
    return -1;
  }
  //检测clientfd是否可写
  if (FD_ISSET(clientfd, &writeset)) {
    int err;
    socklen_t len = static_cast<socklen_t>(sizeof(err));
    if (::getsockopt(clientfd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
      std::cout << "getsockop error! errno : " << errno << std::endl;
      close(clientfd);
      return -1;
    }
    //还要继续检查是否出错,因为在linux中sock出错也是可写的
    //但windows出错不可写,不用这一步
    if (err == 0)
      std::cout << "connect to server success!" << std::endl;
    else
      std::cout << "connect to server error! errno : " << errno << std::endl;
  } else {
    std::cout << "clientfd not set " << std::endl;
  }
  close(clientfd);
  return 0;
}