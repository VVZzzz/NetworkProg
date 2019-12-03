/**
 * select模型,I/O复用,可以非阻塞(不用卡在accept recv等)
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <vector>

int main(int argc,char **argv) {
    //1.----创建监听socket----
    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1) {
        std::cout<<"create listen socket error!"<<std::endl;
        return -1;
    }

    //2.----初始化服务器地址----
    struct sockaddr_in bindaddr;
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(3000);
    if(bind(listenfd,(struct sockaddr *)&bindaddr,sizeof(bindaddr)) == -1) {
        std::cout<<"bind listen socket error!"<<std::endl;
        return -1;
    }

    //3. ----启动监听----
    if(listen(listenfd,SOMAXCONN) == -1) {
        std::cout<<"listen error!"<<std::endl;
        return -1;
    }

    std::cout<<"listenfd is "<<listenfd<<std::endl;

    //存储clientfd
    std::vector<int> clientfds;
    int maxfd = listenfd;

    while (true) {

        fd_set readset;
        FD_ZERO(&readset);

        FD_SET(listenfd,&readset);

        int clientfdslength=clientfds.size();
        for(int i=0;i<clientfdslength;++i) {
            if(clientfds[i] != -1)
            FD_SET(clientfds[i],&readset);
        }

        timeval tm;
        tm.tv_sec=1;
        tm.tv_usec=0;
        //检测可读socket,select会更改fd_set中fd状态以指示
		//最后一个参数有3中可能取值:
		//1. NULL : 阻塞等待,直到有fd就绪
		//2.tv_sec,tv_usec=0,立即返回(即为轮询)
		//3.>0 , 设置一段超时时间.
        int ret = select(maxfd+1,&readset,NULL,NULL,&tm);

        if (ret == -1)
		{
			//出错，退出程序。
			if (errno != EINTR)
				break;
		}
		else if (ret == 0)
		{
			//select 函数超时，下次继续
			continue;
		} else {
            if(FD_ISSET(listenfd,&readset)) {
                //listenfd可读,说明有新连接到来
                struct sockaddr_in clientaddr;
                socklen_t clientaddrlen = sizeof(clientaddr);
                //4. accept接受新连接
                int clientfd = accept(listenfd,(struct sockaddr *)&clientaddr,&clientaddr);
                if(clientfd == -1) {
                    //error
                    break;
                }
                std:: cout << "accept a client connection, fd: " << clientfd << std::endl;
                clientfds.push_back(clientfd);
                if(clientfd > maxfd)
                    maxfd = clientfd;
            } else {
                //无新连接
                char recvbuf[64];
				int clientfdslength = clientfds.size();
                for(int i=0;i<clientfdslength;++i) {
                    if (clientfds[i] != -1 && FD_ISSET(clientfds[i],&readset)) {
                        memset(recvbuf,0,sizeof(recvbuf));
                        //5. recv接收数据
                        int length = recv(clientfds[i],recvbuf,64,0);
                        if(length<=0 && errno != EINTR){
                            std::cout << "recv data error, clientfd: " << clientfds[i] << std::endl;							
							close(clientfds[i]);
							//不直接删除该元素，将该位置的元素置位-1
							clientfds[i] = INVALID_FD;
							continue;
                        }

                        std::cout << "clientfd: " << clientfds[i] << ", recv data: " << recvbuf << std::endl;					

                    }
                }
            }

        }

    }

    //关闭所有客户端socket
    int clientfdslength = clientfds.size();
    for (int i = 0; i < clientfdslength; ++i)
	{
		if (clientfds[i] != INVALID_FD)
		{
			close(clientfds[i]);
		}
	}
    //7. ----关闭侦听socket----
    close(listenfd);
    
    return 0;
}