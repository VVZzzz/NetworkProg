/*
用sigaction实现signal(大多数unix系统都是这样实现的)
为了不使用老的不可靠信号语义的signal
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
typedef void Sigfunc(int);

Sigfunc * signal(int signo , Sigfunc *func) {
    struct sigaction act , oact;
    act.sa_handler = func;
    //首先清空sigaction结构中的屏蔽信号集合
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if(signo == SIGALRM) {
        #ifdef SA_INTERRUPT
        //设置SA_INTERRUPT标志,则不自动重启被信号中断的系统调用
        //对于SIGALRM定时器信号 , 我们不设置自动重启
        act.sa_flags |= SA_INTERRUPT;
        #endif
    } else {
        //设置SA_RESTART标志,则自动重启被信号中断的系统调用
        //对于SIGALARM以外的信号,尝试自动重启
        act.sa_flags |= SA_RESTART;
    }
    if(sigaction(signo,&act,&oact) <0 ) {
        return (SIG_ERR);
    }
    return oact.sa_handler;
}