/*
epoll模型中,设置EPOLLONESHOT标志,
作用是保证被设置EPOLLONESHOT标志的socket可读/写时,
只被一个线程处理.
设置了EPOLLONESHOT标志的事件只会被触发一次!除非用epoll_ctl重置此事件
且最多触发其上注册的一个可读/写/异常事件.
epoll+oneshot+nonblock
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

//传给工作线程的数据结构
struct fds {
  int sockfd;
  int epollfd;
};

//设置非阻塞fd
int setnonblockfd(int fd) {
  int old_socket_flag = fcntl(fd, F_GETFL);
  int new_socket_flag = old_socket_flag | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_socket_flag);
  return old_socket_flag;
}

void addfd(int epollfd, int fd, bool is_one_shot) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET;
  if (is_one_shot) {
    event.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  //设置非阻塞fd
  setnonblockfd(fd);
}

//重置EPOLLONESHOT事件
void reset_oneshot(int epollfd, int fd) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
  //重置这个fd上注册的EPOLLONESHOT事件,以便下次能继续检测到
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
  setnonblockfd(fd);
}

//工作线程
void *worker(void *arg) {
  int sockfd = ((fds *)arg)->sockfd;
  int epollfd = ((fds *)arg)->epollfd;
  std::cout << "start new thread to recv data on fd: " << sockfd << std::endl;
  char buf[1024];

  //由于采取ET触发,读数据时要循环读,直到遇到错误
  while (true) {
    int ret = recv(sockfd, buf, 1023, 0);
    if (ret == 0) {
      //对端关闭
      close(sockfd);
      std::cout << "foreiner close the connection!" << sockfd << std::endl;
      break;
    } else if (ret < 0) {
      //由于是非阻塞socket,没有数据到达时返回EAGIN错误
      if (errno == EAGAIN) {
        reset_oneshot(epollfd, sockfd);
        std::cout << "read later" << std::endl;
        break;
      }
    } else {
      std::cout << "recv data : " << buf << std::endl;
      //该线程休眠2s,代表处理数据
      sleep(2);
    }
  }
  std::cout << "end thread receiving data on fd: " << sockfd << std::endl;
}

int main() {
  // 1.listen
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    std::cout << "listen socket error!" << std::endl;
    return -1;
  }
  // 2.bind severaddr
  struct sockaddr_in serveraddr;
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(3000);

  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) ==
      -1) {
    std::cout << "bind socket error!" << std::endl;
    close(listenfd);
    return -1;
  }

  // 3.listen
  if (listen(listenfd, SOMAXCONN) == -1) {
    std::cout << "bind socket error!" << std::endl;
    close(listenfd);
    return -1;
  }

  // 4.epoll
  epoll_event epoll_events[1024];
  //内核事件表大小为5
  int epollfd = epoll_create(5);
  if (epollfd == -1) {
    std::cout << "bind socket error!" << std::endl;
    close(listenfd);
    return -1;
  }
  //将listenfd和监听事件 添加到内核事件表中
  //注意,listen事件不能设置为EPOLLONESHOT,否则只会处理一个客户端
  addfd(epollfd, listenfd, false);

  while (true) {
    // timeout参数设为-1,一直等待
    int ret = epoll_wait(epollfd, epoll_events, 1024, -1);
    if (ret < 0) {
      std::cout << "epoll error!" << std::endl;
      break;
    }

    //检测事件
    for (int i = 0; i < ret; i++) {
      int fd = epoll_events[i].data.fd;
      if (fd == listenfd) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int clientfd =
            accept(listenfd, (struct sockaddr *)&client_addr, &addr_len);
        //对每个非监听fd都注册EPOLLONESHOT事件
        addfd(epollfd, clientfd, true);
      } else if (epoll_events[i].events & EPOLLIN) {
        //客户端fd有数据发送过来
        pthread_t thread;
        fds fds_for_new_worker;
        fds_for_new_worker.sockfd = fd;
        fds_for_new_worker.epollfd = epollfd;
        //开启新的工作线程为clientfd服务
        pthread_create(&thread, NULL, worker, &fds_for_new_worker);
      } else {
        // other things;
        std::cout << "something happened!" << std::endl;
      }
    }
  }
  close(listenfd);
  return 0;
}
