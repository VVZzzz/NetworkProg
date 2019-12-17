/* SIGURG信号处理带外数据 */
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <vector>

static int connfd;

void sig_urg(int signo) {
  int save_errno = errno;
  printf("in sig_urg: \n");
  char buf[1024];
  memset(buf, '\0', 1024);
  int ret = recv(connfd, buf, 1023, MSG_OOB);  //接收带外数据
  printf("got %d bytes of oob data: %s\n", ret, buf);
  errno = save_errno;
}

void addsig(int signo, void (*sig_handler)(int)) {
  printf("in addsig: \n");
  struct sigaction sa;
  bzero(&sa, sizeof(sa));
  sa.sa_handler = sig_handler;
  sa.sa_flags |= SA_RESTART;  //从信号处理程序返回后,重启系统调用
  sigfillset(&sa.sa_mask);    //屏蔽除signo以外的其他信号
  //sigdelset(&sa.sa_mask,signo);
  sigaction(signo, &sa, NULL);
}
int main(int argc, char **argv) {
  if (argc <= 2) {
    std::cout << "usage: ipaddr  portnumber" << std::endl;
    return 1;
  }
  const char *addr = argv[1];
  int port = atoi(argv[2]);

  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  inet_pton(AF_INET, addr, &serveraddr.sin_addr);

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    return -1;
  }

  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
    return -1;
  }

  if (listen(listenfd, 5) < 0) {
    return -1;
  }


  struct sockaddr_in clientaddr;
  socklen_t len;
  connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &len);

  //安装信号处理程序
  addsig(SIGURG, sig_urg);
  //使用SIGURG之前,必须设置socket的宿主进程或进程组
  // SIGIO和SIGURG信号必须在设置宿主进程后才会产生
  fcntl(connfd, F_SETOWN, getpid());

  char buffer[1024];
  while (true) {
    memset(buffer, '\0', 1024);
    int ret = recv(connfd, buffer, 1023, 0);
    if (ret <= 0) break;
    printf("get %d bytes normal data : '%s'\n", ret, buffer);
  }
  close(connfd);
  close(listenfd);
  return 0;
}