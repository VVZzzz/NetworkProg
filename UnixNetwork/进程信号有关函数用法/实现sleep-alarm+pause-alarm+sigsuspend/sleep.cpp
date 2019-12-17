/*
1.
普通用alarm和pause实现sleep函数,但无法解决竞争条件
2.
用alarm和pause实现sleep函数
其中使用setjmp longjmp解决竞争条件,但会导致被中断的其他信号处理程序
未执行完毕就结束.
3.
使用sigsuspend+alarm实现sleep函数
使用sigsuspend解决竞争条件,且不会导致被中断的其他信号处理程序
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
static jmp_buf env_alarm;

static void sig_int(int sig) {
  for (int i = 0; i < 3000; i++) {
    for (int j = 0; j < 4000; j++) {
      printf("print res : %d\n", i * 3000 + j);
    }
  }
}

/*1. sleep1有问题,若alarm到pause中间被阻塞,执行到pause时,alarm时间已到,那么pause就不是等的定时器事件了*/
static void sig_alarm_1(int sig) { longjmp(env_alarm, 1); }
unsigned int sleep1(unsigned int seconds) {
  if (signal(SIGALRM, sig_alarm_1) == SIG_ERR) {
    return (seconds);
  }

    alarm(seconds);
    pause();

  return (alarm(0));
}

static void sig_alarm_2(int sig) { longjmp(env_alarm, 1); }

//2. 解决alarm和pause竞争条件的sleep函数
unsigned int sleep2(unsigned int seconds) {
  //先安装信号处理程序
  if (signal(SIGALRM, sig_alarm_2) == SIG_ERR) {
    return (seconds);
  }

  if (setjmp(env_alarm) == 0) {
    //直接调用setjmp
    alarm(seconds);
    pause();
  }
  //关闭定时器,并返回未睡眠的时间
  return (alarm(0));
}

//3. sigsuspend实现sleep函数
static void sig_alarm(int signo) {
  //不做任何事情,只用来唤醒sigsuspend
}
unsigned int sleep3(unsigned int seconds) {
  //设置alarm信号处理函数,并保存之前的设置
  struct sigaction alarm_action, alarm_old_action;
  sigset_t alarm_sigset;
  sigemptyset(&alarm_sigset);
  alarm_action.sa_handler = sig_alarm;
  alarm_action.sa_mask = alarm_sigset;
  alarm_action.sa_flags = 0;
  sigaction(SIGALRM,&alarm_action,&alarm_old_action);

  //阻塞alarm信号并保存现在的信号屏蔽字
  sigset_t new_sigset, old_sigset;
  sigemptyset(&new_sigset);
  sigaddset(&new_sigset,SIGALRM);
  sigprocmask(SIG_BLOCK,&new_sigset,&old_sigset);

  //调用alarm
  alarm(seconds);
  sigset_t sus_sigset = old_sigset;  //sigsuspend需要的屏蔽字(即原本的屏蔽字将SIGALRM信号去除)
  sigdelset(&sus_sigset,SIGALRM);
  sigsuspend(&sus_sigset);

  //未sleep的时间
  unsigned int unslept = alarm(0);

  //恢复原本的SIGALRM信号设置
  sigaction(SIGALRM,&alarm_old_action,NULL);
  //恢复原本的屏蔽字
  sigprocmask(SIG_SETMASK,&old_sigset,NULL);

  return(unslept);
}

int main(int argc, char **argv) {
  unsigned int unslept;
  if (signal(SIGINT, sig_int) == SIG_ERR) {
    printf("error!\n");
    exit(-1);
  }
  unslept = sleep2(4);
  printf("unslept seconds: %d\n", unslept);
  exit(0);
}
