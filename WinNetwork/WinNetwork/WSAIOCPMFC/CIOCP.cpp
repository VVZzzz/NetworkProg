#include "stdafx.h"
#include "CIOCP.h"
#include <WS2tcpip.h>
#include "WSAIOCPMFCDlg.h"

CIOCP::CIOCP()
    : m_nPort(DEFAULT_SERVER_PORT),
      m_nThreads(1),
      m_pMainDlg(NULL),
      m_strServerIP(DEFAULT_SERVER_IP),
      m_hShutDownEvent(NULL),
      m_hIOCompletionPort(NULL),
      m_pListenSockinfo(NULL) {}

CIOCP::~CIOCP() {}

bool CIOCP::Start() {
  //��ʼ���ٽ�������(�̻߳������ClientInfoList)
  InitializeCriticalSection(&m_csClientInfoList);

  //�˳�eventΪ�ֶ����ó�ʼ״̬Ϊno-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step I. ��ʼ��IOCP����
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ʧ��! �������:%d!\n"),
                 WSAGetLastError());
    return false;
  } else {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ɹ�!\n"));
  }

  // step II. ��ʼ��listen socket,��ʼ����(bind,listen,Ͷ��Accept����)
  if (false == _InitListenSocket()) {
    _ShowMessage(_T("��ʼ��listen socketʧ��! �������:%d!\n"),
                 WSAGetLastError());
    return false;
  } else {
    _ShowMessage(_T("����listen socket�ɹ�!\n"));
  }

  this->_ShowMessage(_T("ϵͳ׼������,�ȴ�����...\n"));
  return true;
}

void CIOCP::Stop() {
  if (m_hShutDownEvent != NULL && m_phThreads!=NULL) {
    // signal�˳��¼�,ע����workthread�е�WaitSingleObject(m_hShutDownEvent)
    //Ŀ��������,�һ���nThreads���߳�,��Ӧÿһ�̶߳��ᱻpostqueueһ��,���п����е��̱߳����ѵ���û���˳�
    //���Ǽ���whileѭ��,����������GetQueue��,�������൱����һ���߳���������postqueue
    //������һ��event���������Ǹ�ѭ��(��ʹpostqueue������̲߳����˳�,Ҳ������������ѭ��)
    SetEvent(m_hShutDownEvent);

    for (int i = 0; i < m_nThreads; i++)
      //ע�����PostQueued����,�и������õĵط�.ͨ�����ǲ�ʹ��PostQueuedʱ,
      // GetQueued����������������ϵͳ�Զ����������ϵ�,��������ʹ��PostQueuedʱ,
      // GetQueued�������������ξ������Ǵ���PostQueued�ĺ��������.
      //�������ǿ��Զ���CompletionKey����������������Զ��������,Ȼ��GetQueued�յ�������Զ������
      //�����⴦��,�������Ǵ���NULL���������ǵ�EXIT_CODE
      PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE,
                                 NULL);
    //�ȴ������߳��˳�
    WaitForMultipleObjects(m_nThreads, m_phThreads, TRUE, INFINITE);

    //���ClientListInfo
    this->_ClearALLClientListInfo();

    //�ͷ���Դ
    this->_CleanUP();

    this->_ShowMessage(_T("ֹͣ����!"));
  }
}

bool CIOCP::LoadSocketLib() {
  WSADATA wsaData;
  if (SOCKET_ERROR == WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    return false;
  }
  return true;
}

CString CIOCP::GetLocalIP() {
  char hostname[MAX_PATH] = {0};
  gethostname(hostname, MAX_PATH);
  struct hostent *lpHostEnt = gethostbyname(hostname);
  if (NULL == lpHostEnt) return DEFAULT_SERVER_IP;
  LPSTR lpAddr = lpHostEnt->h_addr_list[1];
  struct in_addr inAddr;
  memmove(&inAddr, lpAddr, sizeof(in_addr));
  char ipAddr[30];
  inet_ntop(AF_INET, &inAddr, ipAddr, 30);
  m_strServerIP = CString(ipAddr);
  return m_strServerIP;
}

void CIOCP::_CleanUP() {
  //�ͷ��ٽ�������
  DeleteCriticalSection(&m_csClientInfoList);

  //�ر��˳��¼�
  if (m_hShutDownEvent != NULL) CloseHandle(m_hShutDownEvent);

  //�ر��߳̾��
  for (size_t i = 0; i < m_nThreads; i++) {
    if (NULL != m_phThreads[i] && INVALID_HANDLE_VALUE != m_phThreads[i]) CloseHandle(m_phThreads[i]);
  }
  if (m_phThreads != NULL) {
    delete[] m_phThreads;
    m_phThreads = NULL;
  }

  //�ر���ɶ˿ڶ���
  if (m_hIOCompletionPort != NULL) CloseHandle(m_hIOCompletionPort);

  //�رռ���socket
  if (m_pListenSockinfo != NULL) delete m_pListenSockinfo;

  //���ClientInfoList
  m_listClientSockinfo.clear();
}

void CIOCP::_ShowMessage(const CString strFmt, ...) const {
  va_list pvar;
  CString strmsg;
  va_start(pvar, strFmt);
  strmsg.FormatV(strFmt, pvar);
  va_end(pvar);

  CWSAIOCPMFCDlg *pDlg = (CWSAIOCPMFCDlg *)m_pMainDlg;
  if (pDlg != NULL) pDlg->Addinformation(strmsg);
}

bool CIOCP::_InitIOCompletionPort() {
  //���һ������Ϊ�ö����̴߳���IO
  m_hIOCompletionPort =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
  if (NULL == m_hIOCompletionPort) {
    TRACE(_T("������ɶ˿ڶ���ʧ��!\n"));
    return false;
  }

  //�ܹ����� 2 * ����
  SYSTEM_INFO sysInfo;
  GetSystemInfo(&sysInfo);
  m_nThreads = THREADS_PERPROCESSOR * sysInfo.dwNumberOfProcessors;
  m_phThreads = (HANDLE *)new HANDLE[m_nThreads];

  for (int i = 0; i < m_nThreads; i++) {
    WORKTHREADPARAMS *lpParams = new WORKTHREADPARAMS;
    lpParams->m_iocp = this;
    lpParams->m_nThreadID = i + 1;
    m_phThreads[i] = CreateThread(NULL, 0, _WorkerThreadFunc, (LPVOID)lpParams, 0, NULL);
  }

  TRACE(_T("���� _WorkerThread %d ��.\n"), m_nThreads);
  return true;
}

bool CIOCP::_InitListenSocket() {
  // step 2. ����֧��overlapped��listensocket
  //ʹ��AcceptEx����,MS����ʹ�ú���ָ�����ֱ��ʹ��
  GUID GuidAcceptEx = WSAID_ACCEPTEX;
  GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

  struct sockaddr_in ServerAddr;
  m_pListenSockinfo = new PERSOCKDATA;

  //����socket(֧��overlapped I/O)
  m_pListenSockinfo->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (m_pListenSockinfo->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("��������socketʧ��! �������: %d\n"), WSAGetLastError());
    return false;
  } else {
    TRACE(_T("��������socket�ɹ�!\n"));
  }

  // step 3. ��listen socket�󶨵���ɶ˿ڶ�����
  if (NULL == CreateIoCompletionPort((HANDLE)m_pListenSockinfo->m_Sock,
                                     m_hIOCompletionPort,
                                     (DWORD)m_pListenSockinfo, 0)) {
    _ShowMessage(_T("��listenSocket����ɶ˿ڴ���! �������: %d\n"),
                 WSAGetLastError());
    return false;
  } else {
    TRACE(_T("��listensocket����ɶ˿ڳɹ�!\n"));
  }

  // step4. bind listen
  memset(&ServerAddr, 0, sizeof(ServerAddr));
  ServerAddr.sin_family = AF_INET;
  inet_pton(AF_INET, m_strServerIP.GetString(), &ServerAddr.sin_addr);
  ServerAddr.sin_port = htons(m_nPort);

  if (SOCKET_ERROR == bind(m_pListenSockinfo->m_Sock,
                           (struct sockaddr *)&ServerAddr,
                           sizeof(ServerAddr))) {
    _ShowMessage(_T("bind()ִ�д���! �������: %d\n"), WSAGetLastError());
    return false;
  }

  if (SOCKET_ERROR == listen(m_pListenSockinfo->m_Sock, SOMAXCONN)) {
    _ShowMessage(_T("listen()ִ�д���! �������: %d\n"), WSAGetLastError());
    return false;
  }

  // step5. ʹ��WSAIoctl��ȡAcceptEx����ָ��
  DWORD dwBytes = 0;
  if (SOCKET_ERROR ==
      WSAIoctl(m_pListenSockinfo->m_Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &GuidAcceptEx, sizeof(GuidAcceptEx), &m_lpfnAcceptEx,
               sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL)) {
    _ShowMessage(_T("��ȡAcceptExָ�����! �������: %d\n"), WSAGetLastError());
    return false;
  }

  if (SOCKET_ERROR ==
      WSAIoctl(m_pListenSockinfo->m_Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
               &m_lpfnGetAcceptExSockAddrs, sizeof(m_lpfnGetAcceptExSockAddrs),
               &dwBytes, NULL, NULL)) {
    _ShowMessage(_T("��ȡGetAcceptExSockaddrsָ�����! �������: %d\n"),
                 WSAGetLastError());
    return false;
  }

  // step6. Ͷ��AcceptEx����
  for (int i = 0; i < MAX_POST_ACCEPT; i++) {
    //���һ��AcceptEx����
    PERIODATA *pperiodata = m_pListenSockinfo->GetNewRequest();
    //Ͷ��AcceptEx����
    if (false == _PostAccept(pperiodata)) {
      m_pListenSockinfo->RemoveSpecificRequest(pperiodata);
    }
  }

  return true;
}

bool CIOCP::_PostAccept(PERIODATA *pAcceptIOData) {
  if (INVALID_SOCKET == m_pListenSockinfo->m_Sock) {
    TRACE(_T("Ͷ��Accept�������,ԭ��:δ������Чlisten Socket!\n"));
    return false;
  }
  //׼��AccpetEx������һЩ����
  DWORD dwBytes = 0;
  pAcceptIOData->m_Optype = ACCEPT_POSTED;  // Accept����
  WSABUF *pwsaBuf = &pAcceptIOData->m_Wsabuf;
  OVERLAPPED *poverlapped = &pAcceptIOData->m_Overlapped;

  //��׼����socket,��Ϊ��������AccpetEx,ע���Accept��������
  //ͬ��Ҫ֧��overlapped��socket
  pAcceptIOData->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (pAcceptIOData->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("����accept socketʧ��! �������: %d\n"),
                 WSAGetLastError());
    return false;
  } else {
    TRACE(_T("����accept socket�ɹ�!\n"));
  }

  //Ͷ��AcceptEx����(ʵ�ʲ�������AcceptEx,�����Ĳ�������IO�߳���)
  //ע���ַ��Ϣ������Ҫ������ַ����+16�ֽ�
  if (FALSE == m_lpfnAcceptEx(m_pListenSockinfo->m_Sock, pAcceptIOData->m_Sock,
                              pwsaBuf->buf,
                              pwsaBuf->len - (((sizeof(SOCKADDR_IN)) + 16) * 2),
                              sizeof(SOCKADDR_IN) + 16,
                              sizeof(SOCKADDR_IN) + 16, &dwBytes,
                              poverlapped)) {
    if (WSA_IO_PENDING != WSAGetLastError()) {
      _ShowMessage(_T("Ͷ��AcceptEx�������! �������: %d\n"),
                   WSAGetLastError());
      return false;
    }
  }

  return true;
}

bool CIOCP::_PostRecv(PERIODATA *pAcceptIOData) {
  DWORD dwRecvBytes = 0;
  DWORD dwFlags = 0;
  int nResult = 0;
  //ͬ����������ִ��Recv,Ͷ��Recv����
  nResult = WSARecv(pAcceptIOData->m_Sock, &pAcceptIOData->m_Wsabuf, 1,
                    &dwRecvBytes, &dwFlags, &pAcceptIOData->m_Overlapped, NULL);
  int t = WSAGetLastError();
  if (SOCKET_ERROR == nResult && WSA_IO_PENDING != WSAGetLastError()) {
    this->_ShowMessage(_T("Ͷ��Recv����ʧ��!"));
    return false;
  }
  return true;
}

bool CIOCP::_PostSend(PERIODATA *pAcceptIOData) {
  // pperiodata���ŵ����ϴ��յ�������
  const char *strToClient = " [from server.]\n";
  //������յ��������Ѿ�װ��szbuf,�Ͳ������[from server.]��
  if (strlen(pAcceptIOData->m_szbuf) + strlen(strToClient) + 1 < MAX_BUF_LEN) {
    //-1Ŀ��������\n
    int cpyPos = strlen(pAcceptIOData->m_szbuf) - 1;
    if (cpyPos < 0) cpyPos = 0;
    memmove(pAcceptIOData->m_szbuf + cpyPos, strToClient, strlen(strToClient));
  }
  //Ͷ��Send����
  DWORD dwBytes = 0;
  int nRes = 0;
  nRes = WSASend(pAcceptIOData->m_Sock, &pAcceptIOData->m_Wsabuf, 1, &dwBytes,
                 0, &pAcceptIOData->m_Overlapped, NULL);
  if (SOCKET_ERROR == nRes && WSA_IO_PENDING != WSAGetLastError()) {
    this->_ShowMessage(_T("Ͷ��Recv����ʧ��!"));
    return false;
  }
  return true;
}

void CIOCP::_ClearClientListInfo(PERSOCKDATA *pSockInfo) {
  EnterCriticalSection(&m_csClientInfoList);
  for (auto itr = m_listClientSockinfo.begin();
       itr != m_listClientSockinfo.end(); itr++) {
    if (*itr == pSockInfo) {
      if (pSockInfo != NULL) delete pSockInfo;
      pSockInfo = NULL;
      m_listClientSockinfo.erase(itr);
      break;
    }
  }
  LeaveCriticalSection(&m_csClientInfoList);
}

void CIOCP::_ClearALLClientListInfo() {
  EnterCriticalSection(&m_csClientInfoList);
  for (auto itr = m_listClientSockinfo.begin();
       itr != m_listClientSockinfo.end(); itr++) {
    if (*itr != NULL) delete *itr;
    *itr = NULL;
  }
  m_listClientSockinfo.clear();
  LeaveCriticalSection(&m_csClientInfoList);
}

void CIOCP::_AddClientListInfo(PERSOCKDATA *pSockInfo) {
  EnterCriticalSection(&m_csClientInfoList);
  m_listClientSockinfo.push_back(pSockInfo);
  LeaveCriticalSection(&m_csClientInfoList);
}

bool CIOCP::_DoAccept(PERSOCKDATA *pSockInfo, PERIODATA *pIOInfo) {
  //������ȷpSockInfo��listenSocket,pIOInfo��Я���ͻ���socketIO��Ϣ
  struct sockaddr_in *localAddr = NULL;
  struct sockaddr_in *clientAddr = NULL;
  int localAddrLen = sizeof(SOCKADDR_IN), clientAddrLen = localAddrLen;
  // 1. ȡ������ͻ��˵ĵ�ַ��Ϣ(ʹ��GetAcceptExSockAddrs����)
  this->m_lpfnGetAcceptExSockAddrs(
      pIOInfo->m_Wsabuf.buf,
      pIOInfo->m_Wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
      sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
      (LPSOCKADDR *)&localAddr, &localAddrLen, (LPSOCKADDR *)&clientAddr,
      &clientAddrLen);

  char strclientAddr[30];
  inet_ntop(AF_INET, &clientAddr->sin_addr, strclientAddr, 30);
  this->_ShowMessage("�ͻ��� %s:%d������.", strclientAddr,
                     htons(clientAddr->sin_port));
  this->_ShowMessage("�ͻ��� %s:%d ��Ϣ: %s", strclientAddr,
                     htons(clientAddr->sin_port), pIOInfo->m_szbuf);

  // 2. �����pSockInfo��listensocket,pIOInfoЯ���ͻ��˵���Ϣ.
  //   ����������Ҫ�ڴ���һ��PERSOCKINFO,���SOCKINFO�ǿͻ��˵�,Ȼ�����ACCEPTSOCK�󶨵���ɶ˿�.
  //   ��������Ŀ����ͨ����ɶ˿ڶ����ACCEPTSOCKͶ��recv����,�����յ���Ϣ.
  //   ��listensocketֻ����accept����,���ͻ���(accept
  //   socket)socket����recv��send����

  //�����ͻ��˵�SOCKINFO,��������ɶ˿�
  PERSOCKDATA *pClientSockInfo = new PERSOCKDATA;
  pClientSockInfo->m_Sock = pIOInfo->m_Sock;
  memcpy_s(&pClientSockInfo->m_ClientAddr, sizeof(SOCKADDR_IN), clientAddr,
           sizeof(SOCKADDR_IN));

  //����ɶ˿�
  if (NULL == CreateIoCompletionPort((HANDLE)pClientSockInfo->m_Sock,
                                     m_hIOCompletionPort,
                                     (DWORD)pClientSockInfo, 0)) {
    this->_ShowMessage("ִ��CreateCreateIoCompletionPort����,�������:%d",
                       GetLastError());
    return false;
  }

  // 3. �����ClientSockInfoͶ��recv����,�Ա���������ʱ����.
  //   �ʴ���һ��PERIO����
  PERIODATA *pperiodata = pClientSockInfo->GetNewRequest();
  pperiodata->m_Sock = pClientSockInfo->m_Sock;
  pperiodata->m_Optype = RECV_POSTED;
  //���Ҫ����֮ǰԭ����buffer,��memcpy
  // pperiodata->m_szbuf;

  //Ͷ��recv����
  if (false == _PostRecv(pperiodata)) {
    pClientSockInfo->RemoveSpecificRequest(pperiodata);
    return false;
  }
  //���뵽ClientInfoList��
  _AddClientListInfo(pClientSockInfo);

  // 4.Ͷ��Recv����ɹ�,���pIOInfo�е�buffer(buffer����local
  // ��client��ip����Ϣ)
  //  ���ٴ�Ͷ��AcceptEx����,�Ա����Ӻ���Ŀͻ���.
  pIOInfo->ResetBuf();
  return _PostAccept(pIOInfo);
}

bool CIOCP::_DoRecv(PERSOCKDATA *pSockInfo, PERIODATA *pIOInfo) {
  //������ȷpSockInfo��listenSocket,pIOInfo��Я���ͻ���socketIO��Ϣ
  struct sockaddr_in *clientAddr = &pSockInfo->m_ClientAddr;
  char strClientAddr[30] = {0};
  inet_ntop(AF_INET, &clientAddr->sin_addr, strClientAddr, 30);
  this->_ShowMessage(_T("�յ��ͻ��� %s : %d ������Ϣ %s"), strClientAddr,
                     ntohs(clientAddr->sin_port), pIOInfo->m_szbuf);
  //Ͷ��Send����
  //pIOInfo->ResetBuf();�˴����������,SendҪʹ������
  pIOInfo->m_Optype = SEND_POSTED;
  return _PostSend(pIOInfo);
}

bool CIOCP::_DoSend(PERSOCKDATA *pSockInfo, PERIODATA *pIOInfo) {
  // do noting
  pIOInfo->ResetBuf();
  pIOInfo->m_Optype = RECV_POSTED;
  return _PostRecv(pIOInfo);
}

bool CIOCP::_IsSocketLive(SOCKET sock) {
	int nByteSent=send(sock,"",0,0);
	if (-1 == nByteSent) return false;
	return true;
}

bool CIOCP::_HandleError(PERSOCKDATA *pSockInfo, DWORD dwErrno) {
  switch (dwErrno) {
	  //����ǳ�ʱ
    case WAIT_TIMEOUT:
      if (_IsSocketLive(pSockInfo->m_Sock)) {
        this->_ShowMessage("��⵽�ͻ����쳣�˳�!");
        //��ά����ClientInfoList��ɾ������ͻ���
        this->_ClearClientListInfo(pSockInfo);
        return true;
      } else {
        this->_ShowMessage("���糬ʱ,������...");
        return true;
      }
      //����ǿͻ����쳣
    case ERROR_NETNAME_DELETED:
      this->_ShowMessage("��⵽�ͻ����쳣�˳�!");
      //��ά����ClientInfoList��ɾ������ͻ���
      this->_ClearClientListInfo(pSockInfo);
      return true;
      // IOCPģ�ͳ���,�߳��˳�
    default:
      this->_ShowMessage("��ɶ˿ڲ���ʧ��,�߳��˳�!");
      return false;
  }
}

DWORD __stdcall CIOCP::_WorkerThreadFunc(LPVOID lpParam) {
  WORKTHREADPARAMS *lpWorkThreadPara = (WORKTHREADPARAMS *)lpParam;
  CIOCP *piocp = lpWorkThreadPara->m_iocp;
  int nThreadID = lpWorkThreadPara->m_nThreadID;

  piocp->_ShowMessage("IO�����߳�����,�߳� ID: %d", nThreadID);

  OVERLAPPED *lpoverlapped = NULL;
  PERSOCKDATA *lppersockdata = NULL;
  DWORD dwTransferdBytes = 0;

  while (WAIT_OBJECT_0 != WaitForSingleObject(piocp->m_hShutDownEvent, 0)) {
    //���IO����û�����(��send��recvû�н����ݷ��ͻ������)
	//�̻߳���𵽴˴�(��stopʱ,Ҫ��PostQueuedCompletionStatus����)
    BOOL bReturn = GetQueuedCompletionStatus(
        piocp->m_hIOCompletionPort, &dwTransferdBytes,
        (PULONG_PTR)&lppersockdata, &lpoverlapped, INFINITE);
    //���������߳����˳���־
    if (EXIT_CODE == lppersockdata) {
      break;
    }
    if (!bReturn) {  //����
      DWORD dwErrno = GetLastError();
      if (!(piocp->_HandleError(lppersockdata, dwErrno))) {
		  break;
      }
	  continue;
    } else {
      //�õ�������������ɵ�IO����
      PERIODATA *pperiodata =
          CONTAINING_RECORD(lpoverlapped, PERIODATA, m_Overlapped);
      //��send recv �����ֽ�Ϊ0,˵���Է��ѹر�����
      if (0 == dwTransferdBytes && (RECV_POSTED == pperiodata->m_Optype ||
                                    SEND_POSTED == pperiodata->m_Optype)) {
        char strClientAddr[30] = {0};
        inet_ntop(AF_INET, &(lppersockdata->m_ClientAddr.sin_addr),
                  strClientAddr, 30);

        piocp->_ShowMessage("�ͻ���: %s %d �ѹر�����!", strClientAddr,
                            ntohs(lppersockdata->m_ClientAddr.sin_port));
        //ɾ������ͻ���socket from ClientInfoList
        piocp->_ClearClientListInfo(lppersockdata);
      } else {
        switch (pperiodata->m_Optype) {
          case ACCEPT_POSTED:
            piocp->_DoAccept(lppersockdata, pperiodata);
            break;
          case RECV_POSTED:
            piocp->_DoRecv(lppersockdata, pperiodata);
            break;
          case SEND_POSTED:
            piocp->_DoSend(lppersockdata, pperiodata);
            break;
          default:
            //��Ӧִ�е��˴�
            TRACE(_T("_WorkThreadFunc�е�m_Optype����"));
            break;
        }
      }
    }
  }
  TRACE(_T("�� %d �Ź����߳��˳�.\n"), nThreadID);
  //�ͷ��̲߳���
  if (lpParam != NULL) delete lpParam;
  lpParam = NULL;
  return 0;
}
