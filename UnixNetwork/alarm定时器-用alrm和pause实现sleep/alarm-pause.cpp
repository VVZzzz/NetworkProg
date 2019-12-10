/*
用alarm和pause实现sleep函数
其中使用setjmp longjmp解决竞争条件,但会导致被中断的其他信号处理程序
未执行完毕就结束.
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

/*sleep1有问题,若alarm到pause中间被阻塞,执行到pause时,alarm时间已到,那么pause就不是等的定时器事件了*/
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

//解决alarm和pause竞争条件的sleep函数
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
