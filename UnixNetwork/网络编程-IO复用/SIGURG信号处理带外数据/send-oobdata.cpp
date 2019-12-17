/* 发送带外数据 */
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

  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }
  connect(clientfd, (struct sockaddr *)(&serveraddr), sizeof(serveraddr));
  const char *oob_data = "abc";
  const char *normal_data = "123";
  sleep(1);
  send(clientfd, normal_data, strlen(normal_data), 0);
  sleep(1);
  send(clientfd, oob_data, strlen(oob_data), MSG_OOB);
  //sleep(1);
  send(clientfd, "def", 3, MSG_OOB);
  sleep(1);
  send(clientfd, normal_data, strlen(normal_data), 0);

  close(clientfd);
  return 0;
}