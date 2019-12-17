/*
IO复用系统调用的超时参数
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

int timeout = TIMEOUT;
time_t start = time(NULL);
time_t end = time(NULL);
while(1) {
	printf("the timeout is now %d ms\n",timeout);
	//...
}