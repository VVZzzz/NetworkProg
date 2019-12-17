/*
使用sigsuspend , 进行(sigprocmask(SIG_BLOCK,mask1,mask2)+pause())原子操作
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

//打印当前哪个信号被屏蔽
static void pr_mask() {
  sigset_t oldsigset;
  sigprocmask(0, NULL, &oldsigset);
  if (sigismember(&oldsigset, SIGALRM)) {
    std::cout << "block signal is SIGALRM" << std::endl;
  }
  if (sigismember(&oldsigset, SIGUSR1)) {
    std::cout << "block signal is SIGUSR1" << std::endl;
  }
  if (sigismember(&oldsigset, SIGINT)) {
    std::cout << "block signal is SIGINT" << std::endl;
  }
}
static void sig_int(int signo) {
    std::cout<<"\nin sig_int: "<<std::endl;
    pr_mask();
}

int main(int argc , char**argv) {
    sigset_t newmask,oldmask,waitmask;
    std::cout<<"main start: "<<std::endl;
    signal(SIGINT,sig_int);
    sigemptyset(&waitmask);
    sigemptyset(&newmask);
    sigaddset(&waitmask,SIGUSR1);
    sigaddset(&newmask,SIGINT);

    //设置当前屏蔽信号(屏蔽SIGINT)
    sigprocmask(SIG_BLOCK,&newmask,&oldmask);
    /*临界区代码*/
    std::cout<<"in critical code: "<<std::endl;
    pr_mask();

    //暂停进程,此时只根据waitmask来屏蔽信号
    //即此时只屏蔽SIGUSR1
    sigsuspend(&waitmask);

    std::cout<<"after return from sigsuspend: "<<std::endl;
    pr_mask();
    //返回后,会将当前屏蔽字重新恢复为调用sigsuspend之前的屏蔽字
    //即屏蔽SIGINT

    //恢复到最开始的屏蔽字
    sigprocmask(SIG_SETMASK,&oldmask,NULL);
    std::cout<<"main exit: "<<std::endl;
    pr_mask();
    exit(0);
}

/*输出为:
main start:
in critical code:
block signal is SIGINT
^C
in sig_int:
block signal is SIGUSR1
block signal is SIGINT
after return from sigsuspend:
block signal is SIGINT
main exit:
*/