#include "stdafx.h"
#include "CIOCP.h"
#include "WSAIOCPMFCDlg.h"
#include <WS2tcpip.h>

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
  //�˳�eventΪ�ֶ����ó�ʼ״̬Ϊno-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step I. ��ʼ��IOCP����
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ʧ��! �������:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ɹ�!\n"));
  }

  //step II. ��ʼ��listen socket,��ʼ����(bind,listen,Ͷ��Accept����)
  if (false == _InitListenSocket()) {
    _ShowMessage(_T("��ʼ��listen socketʧ��! �������:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("����listen socket�ɹ�!\n"));
  }


}

void CIOCP::Stop() {
  if (m_hShutDownEvent != NULL) {
	  //signal�˳��¼�
    SetEvent(m_hShutDownEvent);
  }
}

bool CIOCP::LoadSocketLib() {
  WSADATA wsaData;
  if (SOCKET_ERROR == WSAStartup(MAKEWORD(2, 2), &wsaData)) {
    return false;
  }
  return true;
}

void CIOCP::_CleanUP() {
  //�ر��˳��¼�
  if (m_hShutDownEvent != NULL) CloseHandle(m_hShutDownEvent);

  //�ر��߳̾��
  for (size_t i = 0; i < m_nThreads; i++) CloseHandle(m_phThreads[i]);
  if (m_phThreads != NULL) {
    delete[] m_phThreads;
    m_phThreads = NULL;
  }
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
  m_phThreads = new HANDLE[m_nThreads];

  for (int i = 0; i < m_nThreads; i++) {
    WORKTHREADPARAMS *lpParams = new WORKTHREADPARAMS;
    lpParams->m_iocp = this;
    lpParams->m_nThreadID = i + 1;
    CreateThread(NULL,0,xxx,(LPVOID)lpParams,0,NULL);
  }
  
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
}

bool CIOCP::_PostAccept(PERIODATA *pAcceptIOData) { 
	if (INVALID_SOCKET == m_pListenSockinfo->m_Sock) {
    TRACE(_T("Ͷ��Accept�������,ԭ��:δ������Чlisten Socket!\n"));
    return false;
  }
	//׼��AccpetEx������һЩ����
  DWORD dwBytes = 0;
  pAcceptIOData->m_Optype = ACCEPT_POSTED;  //Accept����
  WSABUF *pwsaBuf = &pAcceptIOData->m_Wsabuf;
  OVERLAPPED *poverlapped = &pAcceptIOData->m_Overlapped;

  //��׼����socket,��Ϊ��������AccpetEx,ע���Accept��������
  //ͬ��Ҫ֧��overlapped��socket
  pAcceptIOData->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (pAcceptIOData->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("����accept socketʧ��! �������: %d\n"), WSAGetLastError());
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
