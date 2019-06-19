/**
 * epoll模型,在select模型上改进.仍然属于I/O复用
*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // 1.创建socket
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "create socket error." << std::endl;
    return -1;
  }

  //设置重用地址和端口号
  int on = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(n));
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(n));

  //设置为非阻塞socket
  int oldSockFlag = fcntl(listenfd, F_GETFL, 0);
  int newSockFlag = oldSockFlag | O_NONBLOCK;
  if (fcntl(listenfd, F_SETFL, newSockFlag) == -1) {
    std::cout << "nonblock socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  // bind服务器地址
  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  bindaddr.sin_port = htons(3000);
  if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1) {
    std::cout << "bind socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 2.listen侦听
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "listen socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  //创建epollfd
  int epollfd = epoll_create(1);
  if (epollfd == -1) {
    std::cout << "create epollfd error." << std::endl;
    close(listenfd);
    return -1;
  }

  struct epoll_event listen_fd_event;
  listen_fd_event.data.fd = listenfd;
  //默认epoll events为水平触发
  listen_fd_event.events = EPOLLIN;
  //取消注释,使用边缘出发
  // listen_fd_event.events |= EPOLLET;

  //将listen socket绑定到epollfd上
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &listen_fd_event) == -1) {
    std::cout << "epoll_ctl error." << std::endl;
    close(listenfd);
    return -1;
  }

  int n;
  while (true) {
    epoll_event epoll_events[1024];
    n = epoll_wait(epollfd, epoll_events, 1024, 1000);
    if (n < 0) {
      //被信号中断
      if (errno == EINTR) continue;
      //出错
      break;
    } else if (n == 0) {
      //超时或没有可读数据
      continue;
    }

    for (size_t i = 0; i < n; i++) {
      //事件可读
      if (epoll_events[i].events & EPOLLIN) {
        //如果有新连接
        if (epoll_events[i].data.fd == listenfd) {
          struct sockaddr_in clientaddr;
          socklen_t clientaddrlen = sizeof(clientaddr);
          //由于listenfd是非阻塞的,没有新连接不会阻塞到此处
          int clientfd =
              accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
          if (clientfd != -1) {
            //将clientfd设为非阻塞
            int oldSockFlag = fcntl(clientfd, F_GETFL, 0);
            int newSockFlag = oldSockFlag | O_NONBLOCK;
            if (fcntl(clientfd, F_SETFL, newSockFlag) == -1) {
              std::cout << "nonblock client socket error." << std::endl;
              close(clientfd);
            } else {
              // epoll模型
              struct epoll_event client_fd_event;
              client_fd_event.events = EPOLLIN;
              client_fd_event.data.fd = clientfd;
              client_fd_event.events |= EPOLLET;
              if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd,
                            &client_fd_event) == -1) {
                std::cout << "add client fd to epollfd error" << std::endl;
                close(clientfd);
              } else {
                std::cout << "new client accepted,clientfd: " << clientfd
                          << std::endl;
              }
            }
          }
        }
        // clientfd可读
        else {
          std::cout << "client fd: " << epoll_events[i].data.fd << " recv data."
                    << std::endl;
          char ch;
          int m = recv(epoll_events[i].data.fd, &ch, 1, 0);
          if (m == 0) {
            //对端关闭连接,将clientfd从epollfd中移除(第四个参数忽略)
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_events[i].data.fd,
                          NULL) != -1) {
              std::cout << "client disconnected (socket close), clientfd: "
                        << epoll_events[i].data.fd << std::endl;
            }
            close(epoll_events[i].data.fd);
          } else if (m < 0) {
            if (errno != EWOULDBLOCK && errno != EINTR) {
              //出错.仍移除
              if (epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_events[i].data.fd,
                            NULL) != -1) {
                std::cout << "client disconnected (recv error), clientfd: "
                          << epoll_events[i].data.fd << std::endl;
              }
              close(epoll_events[i].data.fd);
            }
          } else {
            //正常收到数据
            std::cout << "recv from client: " << epoll_events[i].data.fd
                      << " , " << ch << std::endl;
          }
        }
      } else if (epoll_events[i].events & EPOLLERR) {
          //todo epoll出错
      }
    }
  }
  close(listenfd);
  return 0;
}

