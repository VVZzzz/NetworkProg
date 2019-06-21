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
  //退出event为手动重置初始状态为no-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step I. 初始化IOCP对象
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("初始化完成端口对象失败! 错误代码:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("初始化完成端口对象成功!\n"));
  }

  //step II. 初始化listen socket,开始监听(bind,listen,投递Accept请求)
  if (false == _InitListenSocket()) {
    _ShowMessage(_T("初始化listen socket失败! 错误代码:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("开启listen socket成功!\n"));
  }


}

void CIOCP::Stop() {
  if (m_hShutDownEvent != NULL) {
	  //signal退出事件
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
  //关闭退出事件
  if (m_hShutDownEvent != NULL) CloseHandle(m_hShutDownEvent);

  //关闭线程句柄
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
  //最后一个参数为用多少线程处理IO
  m_hIOCompletionPort =
      CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
  if (NULL == m_hIOCompletionPort) {
    TRACE(_T("创建完成端口对象失败!\n"));
    return false;
  }

  //总共开启 2 * 核数
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
  // step 2. 创建支持overlapped的listensocket
  //使用AcceptEx函数,MS建议使用函数指针而非直接使用
  GUID GuidAcceptEx = WSAID_ACCEPTEX;
  GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

  struct sockaddr_in ServerAddr;
  m_pListenSockinfo = new PERSOCKDATA;

  //创建socket(支持overlapped I/O)
  m_pListenSockinfo->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (m_pListenSockinfo->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("创建监听socket失败! 错误代码: %d\n"), WSAGetLastError());
    return false;
  } else {
    TRACE(_T("创建监听socket成功!\n"));
  }

  // step 3. 将listen socket绑定到完成端口对象中
  if (NULL == CreateIoCompletionPort((HANDLE)m_pListenSockinfo->m_Sock,
                                     m_hIOCompletionPort,
                                     (DWORD)m_pListenSockinfo, 0)) {
    _ShowMessage(_T("绑定listenSocket到完成端口错误! 错误代码: %d\n"),
                 WSAGetLastError());
    return false;
  } else {
    TRACE(_T("绑定listensocket到完成端口成功!\n"));
  }

  // step4. bind listen
  memset(&ServerAddr, 0, sizeof(ServerAddr));
  ServerAddr.sin_family = AF_INET;
  inet_pton(AF_INET, m_strServerIP.GetString(), &ServerAddr.sin_addr);
  ServerAddr.sin_port = htons(m_nPort);

  if (SOCKET_ERROR == bind(m_pListenSockinfo->m_Sock,
                           (struct sockaddr *)&ServerAddr,
                           sizeof(ServerAddr))) {
    _ShowMessage(_T("bind()执行错误! 错误代码: %d\n"), WSAGetLastError());
    return false;
  }

  if (SOCKET_ERROR == listen(m_pListenSockinfo->m_Sock, SOMAXCONN)) {
    _ShowMessage(_T("listen()执行错误! 错误代码: %d\n"), WSAGetLastError());
    return false;
  }

  // step5. 使用WSAIoctl获取AcceptEx函数指针
  DWORD dwBytes = 0;
  if (SOCKET_ERROR ==
      WSAIoctl(m_pListenSockinfo->m_Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &GuidAcceptEx, sizeof(GuidAcceptEx), &m_lpfnAcceptEx,
               sizeof(m_lpfnAcceptEx), &dwBytes, NULL, NULL)) {
    _ShowMessage(_T("获取AcceptEx指针错误! 错误代码: %d\n"), WSAGetLastError());
    return false;
  }

  if (SOCKET_ERROR ==
      WSAIoctl(m_pListenSockinfo->m_Sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
               &GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
               &m_lpfnGetAcceptExSockAddrs, sizeof(m_lpfnGetAcceptExSockAddrs),
               &dwBytes, NULL, NULL)) {
    _ShowMessage(_T("获取GetAcceptExSockaddrs指针错误! 错误代码: %d\n"),
                 WSAGetLastError());
    return false;
  }

  // step6. 投递AcceptEx请求
}

bool CIOCP::_PostAccept(PERIODATA *pAcceptIOData) { 
	if (INVALID_SOCKET == m_pListenSockinfo->m_Sock) {
    TRACE(_T("投递Accept请求错误,原因:未创建有效listen Socket!\n"));
    return false;
  }
	//准备AccpetEx函数的一些参数
  DWORD dwBytes = 0;
  pAcceptIOData->m_Optype = ACCEPT_POSTED;  //Accept请求
  WSABUF *pwsaBuf = &pAcceptIOData->m_Wsabuf;
  OVERLAPPED *poverlapped = &pAcceptIOData->m_Overlapped;

  //先准备好socket,作为参数传入AccpetEx,注意和Accept函数区别
  //同样要支持overlapped的socket
  pAcceptIOData->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (pAcceptIOData->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("创建accept socket失败! 错误代码: %d\n"), WSAGetLastError());
    return false;
  } else {
    TRACE(_T("创建accept socket成功!\n"));
  }

  //投递AcceptEx请求(实际并不真正AcceptEx,真正的操作放在IO线程中)
  //注意地址信息长度需要比最大地址长度+16字节
  if (FALSE == m_lpfnAcceptEx(m_pListenSockinfo->m_Sock, pAcceptIOData->m_Sock,
                              pwsaBuf->buf,
                              pwsaBuf->len - (((sizeof(SOCKADDR_IN)) + 16) * 2),
                              sizeof(SOCKADDR_IN) + 16,
                              sizeof(SOCKADDR_IN) + 16, &dwBytes,
                              poverlapped)) {
    if (WSA_IO_PENDING != WSAGetLastError()) {
      _ShowMessage(_T("投递AcceptEx请求错误! 错误代码: %d\n"),
                   WSAGetLastError());
      return false;
    }
  }


  return true;
}
