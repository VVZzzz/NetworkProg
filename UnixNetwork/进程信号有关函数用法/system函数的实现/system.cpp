/*
system函数的实现,这里system1版本没有进行函数处理,不符合POSIX.1要求
system2实现符合POSIX.1的要求
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>
#include <iostream>

int system1(const char *cmdstring) {
  pid_t pid;
  int status;
  if (cmdstring == NULL) {
    return (1);
  }
  if ((pid = fork()) < 0) {
    status = -1;
  } else if (pid == 0) {
    //子进程执行exec
    execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
    _exit(
        127);  // execl失败,若执行excel成功,就不会执行到这一步,因为execl执行成功不返回.
  } else {
    //父进程等待子进程,waitpid会使父进程阻塞在此处
    while (waitpid(pid, &status, 0) < 0)  //等待子进程结束
    {
      if (errno != EINTR) {
        status = -1;
        break;
      }
    }
  }
  return (status);
}

int system2(const char *cmdstring) {
  //带信号处理的system,需要在父进程中忽略SIGINT,SIGQUIT,并阻塞SIGCHLD
  //原因是system执行的可能是交互式程序,且父子进程不能都接收SIGCHLD
  pid_t pid;
  int status;
  struct sigaction ignore, saveintr, savequit;
  sigset_t chldmask, savemask;

  if (cmdstring == NULL) {
    return 1;
  }
  ignore.sa_handler = SIG_IGN;
  sigemptyset(&ignore.sa_mask);
  ignore.sa_flags = 0;

  //忽略SIGINT信号
  if (sigaction(SIGINT, &ignore, &saveintr) < 0) {
    return -1;
  }

  //忽略SIGQUIT信号
  if (sigaction(SIGQUIT, &ignore, &savequit) < 0) {
    return -1;
  }

  //阻塞SIGCHLD信号
  sigemptyset(&chldmask);
  sigaddset(&chldmask, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0) {
    return -1;
  }

  if ((pid = fork()) < 0) {
    status = -1;
  } else if (pid == 0) {
    //子进程,首先恢复到原来的信号屏蔽字
    sigaction(SIGINT, &saveintr, NULL);
    sigaction(SIGQUIT, &savequit, NULL);
    sigprocmask(SIG_SETMASK, &savemask, NULL);
    execl("/bin/sh", "sh", "-c", cmdstring, (char *)0);
    _exit(127);
  } else {
    while (waitpid(pid, &status, 0) < 0) {
      if (errno != EINTR) {
        status = -1;
        break;
      }
    }
  }
  //在父进程中恢复到原来的信号屏蔽字
  if (sigaction(SIGINT, &saveintr, NULL) < 0) return -1;
  if (sigaction(SIGQUIT, &savequit, NULL)) return -1;
  if (sigprocmask(SIG_SETMASK, &savemask, NULL)) return -1;
  return status;
}