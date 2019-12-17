/*
XSI 共享存储进行进程间通信
1. shmget
2. shmctl
3. shmat
4. shmdat
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <iostream>
#define ARRAY_SIZE 4000
#define MALLOC_SIZE 10000
#define SHM_SIZE 10000
#define SHM_MODE 0600  // user read/write 权限

int main(int argc, char **argv) {
  int shmid;
  char *ptr, *shmptr;

  ptr = (char *)malloc(MALLOC_SIZE);
  printf("malloc from %p to %p\n", (void *)ptr, (void *)(ptr + MALLOC_SIZE));

  //得到一个共享存储的标识符(第二个参数为存储大小)
  //且大小如果不是页大小的整数倍,则上取整为页的整数倍.
  //多出来的空间不可用
  shmid = shmget(IPC_PRIVATE, SHM_SIZE, SHM_MODE);

  //令当前进程连接此共享存储
  //最后一个参数为flag: SHM_RND SHM_RDONLY
  shmptr = (char *)shmat(shmid, 0, 0);

  printf("shared memory form %p to %p", (void *)shmptr,
         (void *)(shmptr + SHM_SIZE));

  //用完共享存储之后,删除
  //注意,IPC_RMID并非删除这块共享存储,而是计数减1
  //当连接到此共享存储的进程数为0时,则删除这块共享存储
  shmctl(shmid, IPC_RMID, 0);
}