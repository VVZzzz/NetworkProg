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

  //初始化临界区对象(线程互斥访问ClientInfoList)
  InitializeCriticalSection(&m_csClientInfoList);

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
  //释放临界区对象
  DeleteCriticalSection(&m_csClientInfoList);
  
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
  for (int i = 0; i < MAX_POST_ACCEPT; i++) {
	  //添加一个AcceptEx请求
    PERIODATA *pperiodata = m_pListenSockinfo->GetNewRequest();
	//投递AcceptEx请求
    if (false == _PostAccept(pperiodata)) {
      m_pListenSockinfo->RemoveSpecificRequest(pperiodata);
    }
  }

  return true;
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

bool CIOCP::_PostRecv(PERIODATA *pAcceptIOData) { 
	
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
	//首先明确pSockInfo是listenSocket,pIOInfo是携带客户端socketIO信息
  struct sockaddr_in *localAddr = NULL;
  struct sockaddr_in *clientAddr = NULL;
  int localAddrLen = sizeof(SOCKADDR_IN), clientAddrLen = localAddrLen;
	//1. 取得连入客户端的地址信息(使用GetAcceptExSockAddrs函数)
  this->m_lpfnGetAcceptExSockAddrs(
      pIOInfo->m_Wsabuf.buf,
      pIOInfo->m_Wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
      sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
      (LPSOCKADDR *)&localAddr, &localAddrLen, (LPSOCKADDR *)&clientAddr,
      &clientAddrLen);

  char strclientAddr[30];
  inet_pton(AF_INET, strclientAddr, &clientAddr->sin_addr);
  this->_ShowMessage("客户端 %s:%d已连入.", strclientAddr,
                     htons(clientAddr->sin_port));
  this->_ShowMessage("客户端 %s:%d 信息: ", strclientAddr,
                     htons(clientAddr->sin_port));

  //2. 传入的pSockInfo是listensocket,pIOInfo携带客户端的信息.
  //   所以我们需要在创建一个PERSOCKINFO,这个SOCKINFO是客户端的,然后将这个ACCEPTSOCK绑定到完成端口.
  //   这样做的目的是通过完成端口对这个ACCEPTSOCK投递recv请求,进而收到消息.
  //   即listensocket只负责accept请求,而客户端(accept socket)socket负责recv和send请求

  //创建客户端的SOCKINFO,用来绑定完成端口
  PERSOCKDATA *pClientSockInfo = new PERSOCKDATA;
  pClientSockInfo->m_Sock = pIOInfo->m_Sock;
  memcpy_s(&pClientSockInfo->m_ClientAddr, sizeof(SOCKADDR_IN), clientAddr,
           sizeof(SOCKADDR_IN));

  //绑定完成端口
  if (NULL == CreateIoCompletionPort((HANDLE)pClientSockInfo->m_Sock,
                                     m_hIOCompletionPort,
                                     (DWORD)pClientSockInfo, 0)) {
    this->_ShowMessage("执行CreateCreateIoCompletionPort错误,错误代码:%d",
                       GetLastError());
    return false;
  }

  //3. 在这个ClientSockInfo投递recv请求,以便有数据来时处理.
  //   故创建一个PERIO请求
  PERIODATA *pperiodata = pClientSockInfo->GetNewRequest();
  pperiodata->m_Sock = pClientSockInfo->m_Sock;
  pperiodata->m_Optype = RECV_POSTED;
  //如果要保存之前原本的buffer,则memcpy
  //pperiodata->m_szbuf;
  
  //投递recv请求

}
	

DWORD __stdcall CIOCP::_WorkerThreadFunc(LPVOID lpParam) { 
  WORKTHREADPARAMS *lpWorkThreadPara = (WORKTHREADPARAMS *)lpParam;
  CIOCP *piocp = lpWorkThreadPara->m_iocp;
  int nThreadID = lpWorkThreadPara->m_nThreadID;

  piocp->_ShowMessage("IO工作线程启动,线程 ID: %d", nThreadID);

  OVERLAPPED *lpoverlapped = NULL;
  PERSOCKDATA *lppersockdata = NULL;
  DWORD dwTransferdBytes = 0;

  while (WAIT_OBJECT_0 != WaitForSingleObject(piocp->m_hShutDownEvent, 0)) {
    BOOL bReturn = GetQueuedCompletionStatus(
        piocp->m_hIOCompletionPort, &dwTransferdBytes, (PULONG_PTR)lppersockdata,
        &lpoverlapped, INFINITE);
	//传给工作线程是退出标志
    if (EXIT_CODE == lppersockdata) {
      break;
    }
    if (!bReturn) {  //出错
      DWORD dwErrno = GetLastError();
	  //todo: 处理这个Erro
    } else {
		//拿到传过来的IO请求数据
      PERIODATA *pperiodata =
          CONTAINING_RECORD(lpoverlapped, PERIODATA, m_Overlapped);
	  //即send recv 传输字节为0,说明对方已关闭连接
      if (0 == dwTransferdBytes && (RECV_POSTED == pperiodata->m_Optype ||
                                    SEND_POSTED == pperiodata->m_Optype)) {
		  
		  piocp->_ShowMessage("客户端: %s %d 已关闭连接!",
                            inet_ntoa(lppersockdata->m_ClientAddr.sin_addr),
                            ntohs(lppersockdata->m_ClientAddr.sin_port));
		  //删除这个客户端socket from ClientInfoList
          piocp->_ClearClientListInfo(lppersockdata);
      } else {  
		  switch (pperiodata->m_Optype) {
          case ACCEPT_POSTED:
            piocp->_DoAccept(lppersockdata,pperiodata);
            break;
          case RECV_POSTED:
            piocp->_DoRecv();
            break;
          case SEND_POSTED:
            piocp->_DoSend();
			  break;
          default:
            TRACE(_T("_WorkThreadFunc中的m_Optype出错"));
            break;
        }

      }
    }
  }
  
}
