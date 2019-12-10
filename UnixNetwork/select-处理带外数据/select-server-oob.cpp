/* select 同时处理普通数据和带外数据 */
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // 1.监听fd
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "can't listen!" << std::endl;
    return -1;
  }
  // 2.初始化服务器地址
  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  bindaddr.sin_port = htons(3000);
  if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1) {
    std::cout << "can't bind!" << std::endl;
    return -1;
  }
  // 3.监听
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "listen error!" << std::endl;
    return -1;
  }

  std::cout << "listenfd is " << listenfd << std::endl;

  //进行select
  //存储客户fd
  std::vector<int> clientfds;
  int maxfd = listenfd;

  //这里fd_set 实际上是long int [16],即总共1024bit
  //读事件fd集合
  fd_set readset;  
  //带外数据fd集合,即若有带外数据到来,这个fdset就会就绪
  fd_set exceptionset;  //oob add

  while (true) {
    //每次都要重置
    FD_ZERO(&readset);
    FD_ZERO(&exceptionset);  //oob add
    //将监听fd置位
    FD_SET(listenfd, &readset);
    int clientfds_length = clientfds.size();
    for (int i = 0; i < clientfds_length; i++) {
      if (clientfds[i] != -1) {
        FD_SET(clientfds[i], &readset);
        FD_SET(clientfds[i],&exceptionset);  //oob add!
      }
    }
    timeval tm;
    tm.tv_sec = 1;
    tm.tv_usec = 0;

    //若time参数为NULL,select一直会阻塞,直到有fd就绪
    //否则,只等待time时间就返回
    int ret = select(maxfd + 1, &readset, nullptr, nullptr, &tm);
    //若有fd就绪,ret大于0.若在time时间内,没有fd就绪,ret为0.
    //若失败,ret为-1.若是由于有信号打断返回-1,且设置errno为EINTR
    if (ret == -1) {
      if (errno == EINTR) {
        std::cout << "singal interrupt!" << std::endl;
        break;
      } else {
        std::cout << "select fail!" << std::endl;
        return -1;
      }
    } else if (ret == 0) {
      continue;
    } else {
      if (FD_ISSET(listenfd, &readset)) {
        //若listenfd就绪,说明有心连接
        struct sockaddr_in clientaddr;
        socklen_t clientaddrlen = sizeof(clientaddr);
        //接受新连接
        int clientfd =
            accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
        if (clientfd == -1) {
          // error
          std::cout << "accept error: " << errno << std::endl;
          break;
        }
        std::cout << "accept new client fd is : " << clientfd << std::endl;
        clientfds.push_back(clientfd);
        if (clientfd > maxfd) maxfd = clientfd;

      } else {
        //无新连接到来,说明是有客户端将数据发送过来
        char recvbuf[64];
        int clientfds_length = clientfds.size();
        for (int i = 0; i < clientfds_length; i++) {
          if (clientfds[i] != -1 && FD_ISSET(clientfds[i], &readset)) {
            //清空缓冲区
            memset(recvbuf, 0, sizeof(recvbuf));
            int length = recv(clientfds[i], recvbuf, 64, 0);
            //<0 出错(但是可能是由于EINTR信号导致的),=0对端关闭连接
            if (length <= 0) {
              if (errno == EINTR) {
                std::cout << "EINTR! : " << clientfds[i] << std::endl;
                //释放资源
                close(clientfds[i]);
                //这里不删除fd,而是置为-1(最好还是删除)
                clientfds[i] = -1;
                break;
              }
              std::cout << "recv data error! clientfd: " << clientfds[i]
                        << std::endl;
              //释放资源
              close(clientfds[i]);
              //这里不删除fd,而是置为-1(最好还是删除)
              clientfds[i] = -1;
              continue;
            }

            std::cout << "client fd : " << clientfds[i]
                      << " recv data : " << recvbuf << std::endl;
          }
          /* 以下是处理带外数据*/
          else if (clientfds[i] != -1 && FD_ISSET(clientfds[i], &exceptionset)) {
            //清空缓冲区
            memset(recvbuf, 0, sizeof(recvbuf));
            //注意这里使用MSG_OOB标志的recv,读取带外数据
            int length = recv(clientfds[i], recvbuf, 64, MSG_OOB);
            //<0 出错(但是可能是由于EINTR信号导致的),=0对端关闭连接
            if (length <= 0) {
              if (errno == EINTR) {
                std::cout << "EINTR! : " << clientfds[i] << std::endl;
                //释放资源
                close(clientfds[i]);
                //这里不删除fd,而是置为-1(最好还是删除)
                clientfds[i] = -1;
                break;
              }
              std::cout << "recv data error! clientfd: " << clientfds[i]
                        << std::endl;
              //释放资源
              close(clientfds[i]);
              //这里不删除fd,而是置为-1(最好还是删除)
              clientfds[i] = -1;
              continue;
            }

            std::cout << "client fd : " << clientfds[i]
                      << " recv oob data : " << recvbuf << std::endl;
          }
        }
      }
    }
  }

  //关闭所有clientfd
  for (auto ele : clientfds) {
    if (ele != -1) {
      close(ele);
    }
  }
  close(listenfd);
  return 0;
}