/*WSAEventSelect异步server模型*/
#include <WinSock2.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

int wmain(int argc, TCHAR* argv[]) {
  // 1.初始化套接字库
  WSADATA wsaData;
  int nError = WSAStartup(MAKEWORD(1, 1), &wsaData);
  if (nError != NO_ERROR) return -1;

  // 2.创建监听socket
  SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, 0);
  SOCKADDR_IN addrSrv;
  addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  addrSrv.sin_family = AF_INET;
  addrSrv.sin_port = htons(6000);

  // 3.bind socket
  if (bind(sockSrv, (sockaddr*)&addrSrv, sizeof(addrSrv)) == SOCKET_ERROR) {
    closesocket(sockSrv);
    return -1;
  }

  // 4. 开启监听
  if (listen(sockSrv, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(sockSrv);
    return -1;
  }

  // WSACreateEvente创建一个手动重置的初始状态为non-signaled的event内核对象(匿名)
  WSAEVENT hListenEvent = WSACreateEvent();
  // select去检测监听socket是否有新连接
  if (WSAEventSelect(sockSrv, hListenEvent, FD_ACCEPT) == -1) {
    WSACloseEvent(hListenEvent);
    closesocket(sockSrv);
    WSACleanup();
    return -1;
  }

  WSAEVENT* pEvents = new WSAEVENT[1];
  pEvents[0] = hListenEvent;
  SOCKET* pSockets = new SOCKET[1];
  pSockets[0] = sockSrv;
  DWORD dwCount = 1;
  bool bNeedToMove;

  while (true) {
    bNeedToMove = false;
    //即WaitForMultipleObjects,等待事件signaled
    DWORD dwResult =
        WSAWaitForMultipleEvents(dwCount, pEvents, FALSE, WSA_INFINITE, FALSE);
    //等待失败
    if (dwResult == WSA_WAIT_FAILED) continue;
    DWORD dwIndex = dwResult - WSA_WAIT_EVENT_0;
    for (DWORD i = 0; i < dwIndex; i++) {
		//通过dwIndex编号找到hEvents数组中的WSAEvent对象，进而找到对应的socket
		//这里hEvents数组只有一个元素
      WSANETWORKEVENTS triggeredEvents;
                if (WSA)
    }
  }
}