/*
abort函数的实现(按POSIX.1实现),注意这里有kill给自身进程发信号的用法.
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

void abort(void) {
  sigset_t mask;
  struct sigaction action;

  // SIGABRT进程是无法被忽略的,所以如果它的处理函数是
  //忽略的,就要改成默认行为.
  sigaction(SIGABRT, NULL, &action);
  if (action.sa_handler == SIG_IGN) {
    action.sa_handler = SIG_DFL;
    sigaction(SIGABRT, &action, NULL);
  }

  if (action.sa_handler == SIG_DFL) {
    //如果是SIGABRT的处理函数是默认行为
    //则要冲洗所有IO流
    fflush(NULL);
  }

  //进程是不能阻塞SIGABRT信号的,如果调用abort之前将这个信号阻塞了
  //重新设置非阻塞
  //并且阻塞所有其他信号,这样干的原因是用在kill那里
  sigfillset(&mask);
  sigdelset(&mask, SIGABRT);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  //此时给自身发送信号
  //注意,kill有个特性: 如果是给调用进程发送信号时,且这个信号是不阻塞的,
  //那么在在kill返回之前,这个信号(或者某个非阻塞,未决的信号)就已经投递到这个进程中了.
  //所以我们将除SIGABRT以外的信号阻塞到,那么kill返回,并开始执行下一句,就说明
  //已经捕捉到了SIGABRT信号,并从信号处理函数返回.
  kill(getpid(), SIGABRT);

  //若执行到此处说明:已经捕捉到了SIGABRT信号,并从信号处理函数返回.
  //注意,走到这一步说明是自定义的信号处理函数,因为默认的处理函数是直接终止进程,不会返回.
  //当然,自定义的信号处理函数可能调用exit或者longjmp等,也不会返回.
  //所以,自定义的处理函数可能又输出多个内容,并有可能屏蔽字,所以得再来一次冲洗IO,并
  //重新设置SIGABRT的处理函数为SIG_DFL
  fflush(NULL);
  action.sa_handler = SIG_DFL;
  sigaction(SIGABRT, &action, NULL);
  sigprocmask(SIG_SETMASK, &mask, NULL);
  kill(getpid(), SIGABRT);  //重新发送信号
  exit(1);  //这一句永远执行不到,因为重新发送信号后,信号处理程序是默认行为,使进程终止
}