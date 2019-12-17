/*
IPC_PRIVATE信号量操作
(使用IPC的操作)
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <iostream>

//需要在应用中自己定义semun联合体,用于semctl
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

int main(int argc, char *argv[]) {
  // semget创建一个IPC对象
  //(semget是创建一个新的信号量集合,msgget是创建一个新的消息队列),返回的是键(key)
  //第一个参数是IPC_PRIVATE,第二个参数是信号量集合中放几个信号量,第三个参数是权限
  //见APUE fig.15-24
  int sem_id = semget(IPC_PRIVATE, 1, 0666);

  //初始化信号量集合
  union semun sem_un;
  sem_un.val = 1;
  semctl(sem_id, 0, SETVAL, sem_un);

  pid_t id = fork();
  if (id < 0)
    return -1;
  else if (id == 0) {
    // child
    std::cout << "child try to get binary sem" << std::endl;

    //获取资源
    pv(sem_id, -1);  //若没有足够的资源则阻塞在此次
    std::cout << "child get the sem and would release it after 5 sec."
              << std::endl;
    sleep(5);
    pv(sem_id, 1);
    exit(0);
  } else {
    // parent
    std::cout << "parent try to get binary sem" << std::endl;

    //获取资源
    pv(sem_id, -1);  //若没有足够的资源则阻塞在此次
    std::cout << "parent get the sem and would release it after 5 sec."
              << std::endl;
    sleep(5);
    pv(sem_id, 1);
  }

  waitpid(id, NULL, 0);

  //注意这里,用完之后一定要删除信号量,立即发生
  //若有其他进程正使用这个信号量,则出错.
  semctl(sem_id, 0, IPC_RMID, sem_un);
}
