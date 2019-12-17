/*
使用/dev/zero存储映射(mmap)实现共享存储
但这种mmap只能用于相关联的进程之间

读/dev/zero这个设备时,它是一个0字节的无限资源
写/dev/zero时,它忽略写向它的数据
*/
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <iostream>

#define NLOOPS 1000
#define SIZE sizeof(long)

static int update(long *ptr) { return ((*ptr)++); }

union semun {
  int val;  // for SETVAL
  struct semid_ds
      *buf;  //每个信号量集合都有一个semid_ds结构,for IPC_STAT and IPC_SET
  unsigned short *array;  // for GETALL and SETALL
};

// pv操作 , op>0 释放资源 , op<0 获取资源
//这里没设置IPC_NOWAIT标志,所有semop会阻塞
void pv(int sem_id, int op) {
  // sem_id:信号量id,op:操作
  struct sembuf sem_b;  // sembuf为信号量操作结构
  sem_b.sem_num = 0;  //操作信号量集合中的哪一个信号量(0~nsems-1)
  sem_b.sem_op = op;
  sem_b.sem_flg = SEM_UNDO;  //设置UNDO标志,是当进程exit时能自动释放资源信号量
  semop(sem_id, &sem_b, 1);
}

int main(int argc, char **argv) {
  int fd = open("/dev/zero", O_RDWR);
  //area初始化为0
  void *area = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (area == MAP_FAILED) return -1;

  //为同步父子进程,这里使用IPC_PRIVATE锁
  int sem_id = semget(IPC_PRIVATE, 1, 0666);

  //初始化信号量集合
  union semun sem_un;
  sem_un.val = 1;
  semctl(sem_id, 0, SETVAL, sem_un);

  //注意这里,一旦/dev/zero映射成功,就要关闭它的fd
  close(fd);

  int pid;
  int counter = 0;
  if ((pid = fork()) < 0)
    return -1;
  else if (pid > 0) {
    for (int i = 0; i < NLOOPS; i += 2) {
      pv(sem_id, -1);  //获取资源
      counter = update((long *)area);
      printf("in parent counter: %d , i: %d\n", counter, i);
      //sleep(1);
      pv(sem_id, 1);  //释放资源
    }
  } else {
    for (int i = 1; i < NLOOPS + 1; i += 2) {
      pv(sem_id, -1);  //获取资源
      counter = update((long *)area);
      printf("in child counter: %d , i: %d\n", counter, i);
      //sleep(1);
      pv(sem_id, 1);  //释放资源
    }
    exit(0);
  }
  semctl(sem_id, 0, IPC_RMID, sem_un);
  exit(0);
}