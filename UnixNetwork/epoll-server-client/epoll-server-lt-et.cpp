/**
 * epoll模型LT与ET触发方式, epoll_server.cpp
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

int main() {
  // 1.listen_socket
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "create listen socket error" << std::endl;
    return -1;
  }

  // 2.设置reuseaddr
  int on = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char*)&on, sizeof(on));

  // 3.监听socket为非阻塞
  int old_socket_flag = fcntl(listenfd, F_GETFL, 0);
  int new_socket_flag = old_socket_flag | O_NONBLOCK;
  if (fcntl(listenfd, F_SETFL, new_socket_flag) == -1) {
    close(listenfd);
    std::cout << "set listenfd to nonblock error" << std::endl;
    return -1;
  }

  // 4.bind
  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  bindaddr.sin_port = htons(3000);

  if (bind(listenfd, (struct sockaddr*)&bindaddr, sizeof(bindaddr)) == -1) {
    std::cout << "bind listen socker error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 5.开始listen
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "listen error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 6.创建epollfd
  // epoll_create实际上是告诉内核,这个内核事件表使用多大的
  //得到的epoll_fd 是这个内核事件表描述符
  int epollfd = epoll_create(1);
  if (epollfd == -1) {
    std::cout << "create epollfd error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 7. 将事件添加到epoll事件表
  /*
  typedef union epoll_data  //注意是个联合体
  {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
  } epoll_data_t;

  struct epoll_event
  {
  uint32_t events;	// Epoll events,
  即POLL函数中的事件,但前面要加个E,并且有额外的EPOLLET,EPOLLONESHOT epoll_data_t
  data;	// User data variable,用户数据 } __EPOLL_PACKED;
  */
  epoll_event listen_event;
  listen_event.data.fd = listenfd;  //用户数据使用fd
  listen_event.events = EPOLLIN;
  //默认为LT,水平触发,添加下面语句则为边缘触发
  listen_event.events |= EPOLLET;

  //将listenfd和绑定的事件 添加到这个事件表(epollfd)中
  // epoll_ctl操作选项只有EPOLL_CTL_ADD EPOLL_CTL_MOD EPOLL_CTL_DEL
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &listen_event) == -1) {
    std::cout << "epoll_ctl error" << std::endl;
    close(listenfd);
    return -1;
  }

  // 8.epoll检测可读事件
  int n;
  while (true) {
    epoll_event epoll_events[1024];
    // epoll_wait用法和poll类似的
    //注意: epoll_event是出参,只包含就绪的事件
    n = epoll_wait(epollfd, epoll_events, 1024, 1000);
    if (n < 0) {
      //被信号中断
      if (errno == EINTR) continue;
      //否则出错
      break;
    } else if (n == 0)
      continue;  //超时
    //检测
    for (size_t i = 0; i < n; i++) {
      //可读
      if (epoll_events[i].events & EPOLLIN) {
        if (epoll_events[i].data.fd == listenfd) {
          //侦听socket,接受新连接
          struct sockaddr_in clientaddr;
          socklen_t clientaddrlen = sizeof(clientaddr);
          int clientfd =
              accept(listenfd, (struct sockaddr*)&clientaddr, &clientaddrlen);
          if (clientfd != -1) {
            int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
            int newSocketFlag = oldSocketFlag | O_NONBLOCK;
            if (fcntl(clientfd, F_SETFD, newSocketFlag) == -1) {
              close(clientfd);
              std::cout << "set clientfd to nonblocking error." << std::endl;
            } else {
              //将客户端fd和可读事件添加入内核事件表中
              epoll_event client_fd_event;
              client_fd_event.data.fd = clientfd;
              client_fd_event.events = EPOLLIN;
              //取消注释这一行，则使用ET模式
              client_fd_event.events |= EPOLLET;
              if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd,
                            &client_fd_event) != -1) {
                std::cout << "new client accepted,clientfd: " << clientfd
                          << std::endl;
              } else {
                std::cout << "add client fd to epollfd error" << std::endl;
                close(clientfd);
              }
            }
          }
        }  // end-listenfd
        else {
          std::cout << "client fd: " << epoll_events[i].data.fd << " recv data."
                    << std::endl;
          //普通clientfd
          char ch;
          //每次只收一个字节
          int m = recv(epoll_events[i].data.fd, &ch, 1, 0);
          if (m == 0) {
            //对端关闭了连接，从epollfd上移除clientfd
            if (epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_events[i].data.fd,
                          NULL) != -1) {
              std::cout << "client disconnected,clientfd:"
                        << epoll_events[i].data.fd << std::endl;
            }
            close(epoll_events[i].data.fd);
          } else if (m < 0) {
            //出错
            if (errno != EWOULDBLOCK && errno != EINTR) {
              if (epoll_ctl(epollfd, EPOLL_CTL_DEL, epoll_events[i].data.fd,
                            NULL) != -1) {
                std::cout << "client disconnected,clientfd:"
                          << epoll_events[i].data.fd << std::endl;
              }
              close(epoll_events[i].data.fd);
            }
          } else {
            //正常收到数据
            std::cout << "recv from client:" << epoll_events[i].data.fd << ", "
                      << ch << std::endl;
          }
        }  // end-clientfd
      }    // end-epollin
      else if (epoll_events[i].events & EPOLLERR) {
        //有错可读
        // todo:donothing;
      }
    }
  }
  close(listenfd);
  return 0;
}

/*
如果使用ET触发,那么，如果使用 epoll
边缘模式去检测数据是否可读，触发可读事件以后， 一定要一次性把 socket
上的数据收取干净才行，也就是说一定要循环调用 recv 函数直到 recv 出错，
错误码是EWOULDBLOCK（EAGAIN 一样）（此时表示 socket 上本次数据已经读完）；
如果使用水平模式，则不用，你可以根据业务一次性收取固定的字节数，或者收完为止。
以下是使用ET触发,接收数据的方法:
bool TcpSession::RecvEtMode()
{
    //每次只收取256个字节
    char buff[256];
    while (true)
    {
        int nRecv = ::recv(clientfd_, buff, 256, 0);
        if (nRecv == -1)
        {
            if (errno == EWOULDBLOCK)
                return true;
            else if (errno == EINTR)
                continue;

            return false;
        }
        //对端关闭了socket
        else if (nRecv == 0)
            return false;

        inputBuffer_.add(buff, (size_t)nRecv);
    }

    return true;
}
*/