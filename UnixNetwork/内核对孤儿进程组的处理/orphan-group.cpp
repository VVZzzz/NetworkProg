/*
内核对于孤儿进程组中的进程(被暂停的孤儿进程)的行为.
*/
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>

// SIGHUP信号处理程序
static void sig_hup(int signo) {
  std::cout << "SIGHUP received pid : " << getpid() << std::endl;
}

//打印调用进程的信息
static void print_ids(char *name) {
  std::cout << "name: "<< name <<" pid = " << getpid() << ",ppid = " << getppid()
            << ",pgrp = " << getpgrp() << ",tpgrp = " << tcgetpgrp(STDIN_FILENO)  //返回前台进程组id
            << std::endl;
  fflush(stdout);
}

int main(int argc , char **argv) {
    char c;
    pid_t pid;

    print_ids("parent");
    if((pid = fork()) < 0) {
        std::cout<<"fork error!"<<std::endl;
        exit(-1);
    } else if(pid > 0) {
        //父进程
        sleep(5);
        //注意,父进程终止后子进程会转为后台进程
        //在这个例子中,父进程终止 , 子进程是孤儿进程组.
        //对于孤儿进程组中暂停的进程 , 内核会首先发一个SIGHUP信号,然后再发一个SIGCONT信号.
    } else {
        //子进程
        print_ids("child");
        //绑定信号处理函数
        signal(SIGHUP,sig_hup);
        //给自己发个SIGTSTP信号,使自己暂停
        kill(getpid(),SIGTSTP);

        //如果会恢复,则会继续向下进行
        print_ids("child");
        if(read(STDIN_FILENO,&c,1)!=1) {
            printf("read error %d on controlling TTY\n",errno);
        }
    }
    exit(0);
}