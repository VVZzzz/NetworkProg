/*
使用alarm信号对connect进行超时设置
*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
// SIGALRM信号处理函数
static void sig_alrm(int signo) {
  //注意这里,return只是单纯的打断connect
  //因为signal对于SIGALRM信号是不设置SO_RESTART标志的,即
  //从该alarm信号处理函数返回后,不自动重启系统调用
  //也就是说,sig_alrm返回后,不再执行connect了,即起到了打断作用
  //此时errno未EINTR
  return;
}

//在原本的connect函数最后加一个nsec超时时间
int connect_timeo(int fd, const struct sockaddr* addr, socklen_t addrlen,
                  int nsec) {
  //保存之前的SIGALRM处理函数
  sighandler_t sigfunc = signal(SIGALRM, sig_alrm);
  //开启定时
  if (alarm(nsec) != 0) {
    // alarm返回之前设置的定时未走完的时间
    //故返回非0的话,说明覆盖了之前的定时器
    std::cout << "connect_timeo: alarm was already set." << std::endl;
  }
  int ret;
  if ((ret = connect(fd, addr, addrlen)) < 0) {
    close(fd);
    if (errno == EINTR) {
      //说明超时了,将errono设置为ETIMEDOUT
      errno = ETIMEDOUT;
    }
  }
  alarm(0);                  //取消这个定时器
  signal(SIGALRM, sigfunc);  //恢复未原来的信号处理函数

  return ret;
}