#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
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

  int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
  int newSocketFlag = oldSocketFlag | O_NONBLOCK;
  //修改clientfd标志位
  if (fcntl(clientfd, F_SETFL, newSocketFlag) == -1) {
    std::cout << "set clientfd nonblock error!" << std::endl;
    close(clientfd);
    return -1;
  }

  // 2. ----connect连接服务器----
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
  serveraddr.sin_port = htons(SERVER_PORT);

  while (true) {
    int ret =
        connect(clientfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if (ret == 0) {
      std::cout << "connect successfully!" << std::endl;
      close(clientfd);
      return 0;
    } else if (ret == -1) {
      if (ret == EINTR) {
        std::cout << "connect interruptted by signal,try again!" << std::endl;
        continue;

      } else if (ret == EINPROGRESS) {
        // connect正在尝试连接,即要在后面查看clientfd是否有效且可写
        break;
      } else {
        //出错
        close(clientfd);
        return -1;
      }
    }
  }

  // poll函数查看clientfd是否可写
  pollfd cilentw;
  cilentw.fd = clientfd;
  cilentw.events = POLLOUT;
  cilentw.revents = 0;
  //等待3000ms
  int timeout = 3000;
  //在3s时间内改clientfd仍不可写,则connect error
  //最后一个timeout参数有3种可能取值:
  //1.>0 设置一段超时时间
  //2.=0 立即返回,即轮询
  //3.INFTIM 阻塞等待,直到有就绪的fd
  if (poll(&cilentw, 1, timeout) != 1) {
    close(clientfd);
    std::cout << "[poll] connect error." << std::endl;
    return -1;
  }

  if (!(cilentw.revents & POLLOUT)) {
    close(clientfd);
    std::cout << "[pollout] connect error." << std::endl;
    return -1;
  }

  //即使可写了,仍要检查这个clientfd是否出错.
  int err;
  socklen_t errlen = static_cast<socklen_t>(sizeof(err));
  if (::getsockopt(clientfd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
    std::cout << "call getsockopt error." << std::endl;
    close(clientfd);
    return -1;
  }

  if (err == 0)
    std::cout << "connect server successfully." << std::endl;
  else
    std::cout << "[err] connect server error." << std::endl;

  // 5. ----关闭socket----
  close(clientfd);
  return 0;
}