/*
使用sigsetjmp-siglongjmp组合进行恢复屏蔽信号集合
很重要嗷
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

static void sig_usr1(int);
static void sig_alarm(int);
static sigjmp_buf jmpbuf;
//标志是否可以跳转
static volatile sig_atomic_t canjump;
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

int main(int argc, char **argv) {
  if (signal(SIGUSR1, sig_usr1) == SIG_ERR) {
    std::cout << "signal error!" << std::endl;
    exit(-1);
  }
  if (signal(SIGALRM, sig_alarm) == SIG_ERR) {
    std::cout << "signal error!" << std::endl;
    exit(-1);
  }
  std::cout << "starting main:" << std::endl;
  pr_mask();
  if (sigsetjmp(jmpbuf, 1)) {
    //由siglongjmp返回后,sigsetjmp返回非0
    std::cout << "ending main: " << std::endl;
    pr_mask();
  }
  //此时sigsetjmp返回点已设置成功
  canjump = 1;
  while (true) {
    pause();  //休眠进程,只有处理信号才可以唤醒
  }
}

static void sig_usr1(int signo) {
  if (canjump == 0) {
    // sigsetjmp没有设置好,不能跳转
    return;
  }
  std::cout << "starting sig_usr1: " << std::endl;
  pr_mask();
  alarm(3);
  time_t starttime = time(NULL);
  //循环5秒
  while (true) {
    if (time(NULL) > starttime + 5) break;
  }
  std::cout << "ending sig_usr1: " << std::endl;
  pr_mask();

  canjump = 0;            //复位
  siglongjmp(jmpbuf, 1);  //跳转到sigsetjmp处
}
static void sig_alarm(int signo) {
  std::cout << "in sig_alarm: " << std::endl;
  pr_mask();
}

/*输出情况为:
14:51 run@run_ubuntu:~/projects $./sigsetjmp-siglongjmp &
[2] 71687
14:51 run@run_ubuntu:~/projects $starting main:  //初始没有屏蔽的信号
kill -USR1 71687
starting sig_usr1:
block signal is SIGUSR1
14:51 run@run_ubuntu:~/projects $in sig_alarm:
block signal is SIGALRM   //此时有俩屏蔽信号
block signal is SIGUSR1
ending sig_usr1:
block signal is SIGUSR1  //由于sig_alarm返回,SIGALRM信号不再屏蔽
ending main:             //由于sigsetjmp恢复初始的信号集,即没有屏蔽的信号
 */

//如果将sigsetjmp,siglongjmp换为setjmp,longjmp,则最后会输出
//ending main:             
//block signal is SIGUSR1