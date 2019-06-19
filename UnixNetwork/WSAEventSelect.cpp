/*WSAEventSelect�첽serverģ��*/
#include <WinSock2.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

int wmain(int argc, TCHAR* argv[]) {
  // 1.��ʼ���׽��ֿ�
  WSADATA wsaData;
  int nError = WSAStartup(MAKEWORD(1, 1), &wsaData);
  if (nError != NO_ERROR) return -1;

  // 2.��������socket
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

  // 4. ��������
  if (listen(sockSrv, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(sockSrv);
    return -1;
  }

  // WSACreateEvente����һ���ֶ����õĳ�ʼ״̬Ϊnon-signaled��event�ں˶���(����)
  WSAEVENT hListenEvent = WSACreateEvent();
  // selectȥ������socket�Ƿ���������
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
    //��WaitForMultipleObjects,�ȴ��¼�signaled
    DWORD dwResult =
        WSAWaitForMultipleEvents(dwCount, pEvents, FALSE, WSA_INFINITE, FALSE);
    //�ȴ�ʧ��
    if (dwResult == WSA_WAIT_FAILED) continue;
    DWORD dwIndex = dwResult - WSA_WAIT_EVENT_0;
    for (DWORD i = 0; i < dwIndex; i++) {
		//ͨ��dwIndex����ҵ�hEvents�����е�WSAEvent���󣬽����ҵ���Ӧ��socket
		//����hEvents����ֻ��һ��Ԫ��
      WSANETWORKEVENTS triggeredEvents;
                if (WSA)
    }
  }
}