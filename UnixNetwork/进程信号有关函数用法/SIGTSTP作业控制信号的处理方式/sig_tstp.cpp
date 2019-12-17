/*
作业控制信号有: SIGCHLD,SIGCONT,SIGSTOP,SIGTSTP,SIGTTIN,SIGTTOU
SIGTSTP为交互式暂停,以下显示了如何处理该信号
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

static void sig_tstp(int signo) {
    sigset_t mask;
    //首先将光标移到最左,并重设控制终端模式(reset tty mode)
    //接着接触阻塞SIGTSTP信号
    sigemptyset(&mask);
    sigaddset(&mask,SIGTSTP);
    sigprocmask(SIG_UNBLOCK,&mask,NULL);

    //此时再重新设置SIGTSTP为默认行为(即暂停进程)
    signal(SIGTSTP,SIG_DFL);
    //给自己发SIGTSTP
    kill(getpid(),SIGTSTP);

    //kill给自己发非阻塞信号时,在返回前进程就会收到信号
    //所以此例中由于收到了SIGTSTP,默认行为时暂停,所以就不会往下执行
    //除非收到SIGCONT
    signal(SIGTSTP,sig_tstp);  //重新恢复信号处理函数

    //重设tty mode,并重绘屏幕
}