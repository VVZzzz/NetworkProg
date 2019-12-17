/*
sigprocmask和sigpending等信号集函数
对于阻塞(屏蔽)未决信号的行为
*/
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>

static void sig_quit(int signo) {
    printf("SIGQUIT caught!\n");
    //重置SIGQUIT默认行为(使进程终止)
    signal(SIGQUIT,SIG_DFL);
}

int main(int argc, char **argv) {
    sigset_t old_sigset,new_sigset,pend_sigset;
    //先安装信号处理程序
    signal(SIGQUIT,sig_quit);
    
    sigemptyset(&new_sigset);
    sigaddset(&new_sigset,SIGQUIT);
    //将new_sigset中的信号,即SIGQUIT设为阻塞(屏蔽),old_sigset返回进程原来的信号集
    sigprocmask(SIG_BLOCK,&new_sigset,&old_sigset);

    //休眠,此时SIGQUIT是阻塞的
    //此时按下ctrl+\发送SIGQUIT信号,是无法投递的,即此时该信号是未决的
    sleep(5);

    //获取当前阻塞的无法投递的信号(也就是未决的)
    sigpending(&pend_sigset);
    if(sigismember(&pend_sigset,SIGQUIT)) {
        std::cout<<"SIGQUIT pending"<<std::endl;
    }

    //注意这个sigprocmask做两件事
    //1.首先将当前的信号集设为old_sigset,也就是恢复为最原来的,那么SIGQUIT信号此时被设为非阻塞的了
    //2.接着会将此时进程中非阻塞且未决的信号发送给进程,即此时SIGQUIT就是非阻塞未决的状态,被发送
    sigprocmask(SIG_SETMASK,&old_sigset,NULL);

    //此时SIGQUIT是未阻塞的
    std::cout<<"SIGQUIT unblock"<<std::endl;

    sleep(5);
    exit(0);
}

/*在第一次sleep时,我们发送10次,ctrl+\,
  结果只会打印一次"SIGQUIT caught",说明信号不排队.
  并且最后会产生(coredump),这是由于执行完sig_quit后,SIGQUIT恢复为默认行为,即进程结束.
  所以在sigprocmask对进程发送SIGQUIT信号时,进程直接终止
*/