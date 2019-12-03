#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  // 1.listen socket (nonblock)
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "create listen socket error." << std::endl;
    return -1;
  }

  int oldSocketFlag = fcntl(listenfd, F_GETFL, 0);
  int newSocketFlag = oldSocketFlag | O_NONBLOCK;
  if (fcntl(listenfd, F_SETFL, newSocketFlag) == -1) {
    std::cout << "listen socket nonblock error." << std::endl;
    close(listenfd);
    return -1;
  }

  //复用地址和端口号
  int on = 1;//设置选项标志置位(不为0则置位)
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on));

  // 2. bind client sockaddr_in
  struct sockaddr_in bindaddr;
  bindaddr.sin_family = AF_INET;
  bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  bindaddr.sin_port = htons(3000);
  if (bind(listenfd, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) == -1) {
    std::cout << "bind listen socket error." << std::endl;
    close(listenfd);
    return -1;
  }

  // 3. listen
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "listen error." << std::endl;
    close(listenfd);
    return -1;
  }

  std::vector<pollfd> fds;
  struct pollfd listen_fd_info;
  listen_fd_info.fd = listenfd;
  //检查可读事件
  listen_fd_info.events = POLLIN;
  listen_fd_info.revents = 0;
  fds.push_back(listen_fd_info);

  //是否存在无效fd
  bool exist_invalid_fd;
  int n;
  while (true) {
    exist_invalid_fd = false;
	  //1.>0 设置一段超时时间
	  //2.=0 立即返回,即轮询
	  //3.INFTIM 阻塞等待,直到有就绪的fd
    n = poll(&fds[0], fds.size(), 1000);
    if (n < 0) {
      //信号中断
      if (errno == EINTR) continue;
      //出错
      break;
    } else if (n == 0) {
      //在timeout时间内无可读事件
      //继续等待
      continue;
    }

    for (size_t i = 0; i < fds.size(); i++) {
      //事件可读
      if (fds[i].revents & POLLIN) {
        // 如果是listensocket可读,说明有新连接
        if (fds[i].fd == listenfd) {
          struct sockaddr_in clientaddr;
          socklen_t clientaddrlen = sizeof(clientaddr);
          int clientfd =
              accept(listenfd, (struct sockaddr *)&clientaddr, &clientaddrlen);
          if (clientfd != -1) {
            // clientsocket nonblock
            int oldSocketFlag = fcntl(clientfd, F_GETFL, 0);
            int newSocketFlag = oldSocketFlag | O_NONBLOCK;
            if (fcntl(clientfd, F_SETFL, newSocketFlag) == -1) {
              std::cout << "client socket nonblock error." << std::endl;
              close(clientfd);
            } else {
              //将当前clientfd添加到poll中
              struct pollfd client_fd_info;
              client_fd_info.fd = clientfd;
              client_fd_info.events = POLLIN;
              client_fd_info.revents = 0;
              fds.push_back(client_fd_info);
              std::cout << "new client acceptted , clientfd: " << clientfd
                        << std::endl;
            }
          }
        } else {
          // clientfd可读,有新数据
          char buf[64] = {0};
          //注意clientsocket是非阻塞的
          int ret = recv(fds[i].fd, buf, 64, 0);
          if (ret <= 0) {
            if (ret != EINTR && ret != EWOULDBLOCK) {
              std::cout << "client disconnected ,clientfd: " << fds[i].fd
                        << std::endl;
              // recv出错或者对端关闭连接
              //关闭这个clientfd
              close(fds[i].fd);
              //设为无效fd
              fds[i].fd = -1;
              exist_invalid_fd = true;
              break;
            }
          } else {
            std::cout << "recv from client : " << buf
                      << ", clientfd: " << fds[i].fd << std::endl;
          }
        }
      } else if (fds[i].revents & POLLERR) {
        // todo:发生错误.
      }
    }

    //统一删除无效fd
    if (exist_invalid_fd) {
      for (std::vector<pollfd>::iterator iter = fds.begin();
           iter != fds.end();) {
        if (iter->fd == -1)
          iter = fds.erase(iter);
        else
          iter++;
      }
    }
  }

  //关闭所有socket
  for (size_t i = 0; i < fds.size(); i++) close(fds[i].fd);
  return 0;
}