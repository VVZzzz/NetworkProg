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
  //初始化临界区对象(线程互斥访问ClientInfoList)
  InitializeCriticalSection(&m_csClientInfoList);

  //退出event为手动重置初始状态为no-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step I. 初始化IOCP对象
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("初始化完成端口对象失败! 错误代码:%d!\n"),
                 WSAGetLastError());
    return false;
  } else {
    _ShowMessage(_T("初始化完成端口对象成功!\n"));
  }

  // step II. 初始化listen socket,开始监听(bind,listen,投递Accept请求)
  if (false == _InitListenSocket()) {
    _ShowMessage(_T("初始化listen socket失败! 错误代码:%d!\n"),
                 WSAGetLastError());
    return false;
  } else {
    _ShowMessage(_T("开启listen socket成功!\n"));
  }

  this->_ShowMessage(_T("系统准备就绪,等待连接...\n"));
  return true;
}

void CIOCP::Stop() {
  if (m_hShutDownEvent != NULL && m_phThreads!=NULL) {
    // signal退出事件,注意在workthread中的WaitSingleObject(m_hShutDownEvent)
    //目的是下面,我唤醒nThreads个线程,理应每一线程都会被postqueue一次,但有可能有的线程被唤醒但是没有退出
    //而是继续while循环,继续挂起在GetQueue处,这样就相当于它一个线程拿了两次postqueue
    //故设置一个event用来跳出那个循环(即使postqueue对这个线程不能退出,也可以让他跳出循环)
    SetEvent(m_hShutDownEvent);

    for (int i = 0; i < m_nThreads; i++)
      //注意这个PostQueued函数,有个很有用的地方.通常我们不使用PostQueued时,
      // GetQueued函数后三个出参是系统自动帮我们填上的,但是我们使用PostQueued时,
      // GetQueued函数后三个出参就是我们传入PostQueued的后三个入参.
      //所以我们可以对于CompletionKey这个参数传入我们自定义的数据,然后GetQueued收到对这个自定义情况
      //做特殊处理,这里我们传入NULL即当作我们的EXIT_CODE
      PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)EXIT_CODE,
                                 NULL);
    //等待所有线程退出
    WaitForMultipleObjects(m_nThreads, m_phThreads, TRUE, INFINITE);

    //清除ClientListInfo
    this->_ClearALLClientListInfo();

    //释放资源
    this->_CleanUP();

    this->_ShowMessage(_T("停止监听!"));
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
  //释放临界区对象
  DeleteCriticalSection(&m_csClientInfoList);

  //关闭退出事件
  if (m_hShutDownEvent != NULL) CloseHandle(m_hShutDownEvent);

  //关闭线程句柄
  for (size_t i = 0; i < m_nThreads; i++) {
    if (NULL != m_phThreads[i] && INVALID_HANDLE_VALUE != m_phThreads[i]) CloseHandle(m_phThreads[i]);
  }
  if (m_phThreads != NULL) {
    delete[] m_phThreads;
    m_phThreads = NULL;
  }

  //关闭完成端口对象
  if (m_hIOCompletionPort != NULL) CloseHandle(m_hIOCompletionPort);

  //关闭监听socket
  if (m_pListenSockinfo != NULL) delete m_pListenSockinfo;

  //清空ClientInfoList
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
  m_phThreads = (HANDLE *)new HANDLE[m_nThreads];

  for (int i = 0; i < m_nThreads; i++) {
    WORKTHREADPARAMS *lpParams = new WORKTHREADPARAMS;
    lpParams->m_iocp = this;
    lpParams->m_nThreadID = i + 1;
    m_phThreads[i] = CreateThread(NULL, 0, _WorkerThreadFunc, (LPVOID)lpParams, 0, NULL);
  }

  TRACE(_T("建立 _WorkerThread %d 个.\n"), m_nThreads);
  return true;
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
  pAcceptIOData->m_Optype = ACCEPT_POSTED;  // Accept请求
  WSABUF *pwsaBuf = &pAcceptIOData->m_Wsabuf;
  OVERLAPPED *poverlapped = &pAcceptIOData->m_Overlapped;

  //先准备好socket,作为参数传入AccpetEx,注意和Accept函数区别
  //同样要支持overlapped的socket
  pAcceptIOData->m_Sock =
      WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (pAcceptIOData->m_Sock == INVALID_SOCKET) {
    _ShowMessage(_T("创建accept socket失败! 错误代码: %d\n"),
                 WSAGetLastError());
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
  DWORD dwRecvBytes = 0;
  DWORD dwFlags = 0;
  int nResult = 0;
  //同样并不真正执行Recv,投递Recv请求
  nResult = WSARecv(pAcceptIOData->m_Sock, &pAcceptIOData->m_Wsabuf, 1,
                    &dwRecvBytes, &dwFlags, &pAcceptIOData->m_Overlapped, NULL);
  int t = WSAGetLastError();
  if (SOCKET_ERROR == nResult && WSA_IO_PENDING != WSAGetLastError()) {
    this->_ShowMessage(_T("投递Recv请求失败!"));
    return false;
  }
  return true;
}

bool CIOCP::_PostSend(PERIODATA *pAcceptIOData) {
  // pperiodata里存放的是上次收到的数据
  const char *strToClient = " [from server.]\n";
  //如果接收到的数据已经装满szbuf,就不再添加[from server.]了
  if (strlen(pAcceptIOData->m_szbuf) + strlen(strToClient) + 1 < MAX_BUF_LEN) {
    //-1目的是消除\n
    int cpyPos = strlen(pAcceptIOData->m_szbuf) - 1;
    if (cpyPos < 0) cpyPos = 0;
    memmove(pAcceptIOData->m_szbuf + cpyPos, strToClient, strlen(strToClient));
  }
  //投递Send请求
  DWORD dwBytes = 0;
  int nRes = 0;
  nRes = WSASend(pAcceptIOData->m_Sock, &pAcceptIOData->m_Wsabuf, 1, &dwBytes,
                 0, &pAcceptIOData->m_Overlapped, NULL);
  if (SOCKET_ERROR == nRes && WSA_IO_PENDING != WSAGetLastError()) {
    this->_ShowMessage(_T("投递Recv请求失败!"));
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
  //首先明确pSockInfo是listenSocket,pIOInfo是携带客户端socketIO信息
  struct sockaddr_in *localAddr = NULL;
  struct sockaddr_in *clientAddr = NULL;
  int localAddrLen = sizeof(SOCKADDR_IN), clientAddrLen = localAddrLen;
  // 1. 取得连入客户端的地址信息(使用GetAcceptExSockAddrs函数)
  this->m_lpfnGetAcceptExSockAddrs(
      pIOInfo->m_Wsabuf.buf,
      pIOInfo->m_Wsabuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
      sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
      (LPSOCKADDR *)&localAddr, &localAddrLen, (LPSOCKADDR *)&clientAddr,
      &clientAddrLen);

  char strclientAddr[30];
  inet_ntop(AF_INET, &clientAddr->sin_addr, strclientAddr, 30);
  this->_ShowMessage("客户端 %s:%d已连入.", strclientAddr,
                     htons(clientAddr->sin_port));
  this->_ShowMessage("客户端 %s:%d 信息: %s", strclientAddr,
                     htons(clientAddr->sin_port), pIOInfo->m_szbuf);

  // 2. 传入的pSockInfo是listensocket,pIOInfo携带客户端的信息.
  //   所以我们需要在创建一个PERSOCKINFO,这个SOCKINFO是客户端的,然后将这个ACCEPTSOCK绑定到完成端口.
  //   这样做的目的是通过完成端口对这个ACCEPTSOCK投递recv请求,进而收到消息.
  //   即listensocket只负责accept请求,而客户端(accept
  //   socket)socket负责recv和send请求

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

  // 3. 在这个ClientSockInfo投递recv请求,以便有数据来时处理.
  //   故创建一个PERIO请求
  PERIODATA *pperiodata = pClientSockInfo->GetNewRequest();
  pperiodata->m_Sock = pClientSockInfo->m_Sock;
  pperiodata->m_Optype = RECV_POSTED;
  //如果要保存之前原本的buffer,则memcpy
  // pperiodata->m_szbuf;

  //投递recv请求
  if (false == _PostRecv(pperiodata)) {
    pClientSockInfo->RemoveSpecificRequest(pperiodata);
    return false;
  }
  //加入到ClientInfoList中
  _AddClientListInfo(pClientSockInfo);

  // 4.投递Recv请求成功,清空pIOInfo中的buffer(buffer保存local
  // 和client的ip及信息)
  //  并再次投递AcceptEx请求,以便连接后面的客户端.
  pIOInfo->ResetBuf();
  return _PostAccept(pIOInfo);
}

bool CIOCP::_DoRecv(PERSOCKDATA *pSockInfo, PERIODATA *pIOInfo) {
  //首先明确pSockInfo是listenSocket,pIOInfo是携带客户端socketIO信息
  struct sockaddr_in *clientAddr = &pSockInfo->m_ClientAddr;
  char strClientAddr[30] = {0};
  inet_ntop(AF_INET, &clientAddr->sin_addr, strClientAddr, 30);
  this->_ShowMessage(_T("收到客户端 %s : %d 发来信息 %s"), strClientAddr,
                     ntohs(clientAddr->sin_port), pIOInfo->m_szbuf);
  //投递Send请求
  //pIOInfo->ResetBuf();此处不进行清空,Send要使用数据
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
	  //如果是超时
    case WAIT_TIMEOUT:
      if (_IsSocketLive(pSockInfo->m_Sock)) {
        this->_ShowMessage("检测到客户端异常退出!");
        //从维护的ClientInfoList中删除这个客户端
        this->_ClearClientListInfo(pSockInfo);
        return true;
      } else {
        this->_ShowMessage("网络超时,重试中...");
        return true;
      }
      //如果是客户端异常
    case ERROR_NETNAME_DELETED:
      this->_ShowMessage("检测到客户端异常退出!");
      //从维护的ClientInfoList中删除这个客户端
      this->_ClearClientListInfo(pSockInfo);
      return true;
      // IOCP模型出错,线程退出
    default:
      this->_ShowMessage("完成端口操作失败,线程退出!");
      return false;
  }
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
    //如果IO操作没有完成(即send和recv没有将数据发送或接收完)
	//线程会挂起到此处(故stop时,要用PostQueuedCompletionStatus唤醒)
    BOOL bReturn = GetQueuedCompletionStatus(
        piocp->m_hIOCompletionPort, &dwTransferdBytes,
        (PULONG_PTR)&lppersockdata, &lpoverlapped, INFINITE);
    //传给工作线程是退出标志
    if (EXIT_CODE == lppersockdata) {
      break;
    }
    if (!bReturn) {  //出错
      DWORD dwErrno = GetLastError();
      if (!(piocp->_HandleError(lppersockdata, dwErrno))) {
		  break;
      }
	  continue;
    } else {
      //拿到传过来的已完成的IO数据
      PERIODATA *pperiodata =
          CONTAINING_RECORD(lpoverlapped, PERIODATA, m_Overlapped);
      //即send recv 传输字节为0,说明对方已关闭连接
      if (0 == dwTransferdBytes && (RECV_POSTED == pperiodata->m_Optype ||
                                    SEND_POSTED == pperiodata->m_Optype)) {
        char strClientAddr[30] = {0};
        inet_ntop(AF_INET, &(lppersockdata->m_ClientAddr.sin_addr),
                  strClientAddr, 30);

        piocp->_ShowMessage("客户端: %s %d 已关闭连接!", strClientAddr,
                            ntohs(lppersockdata->m_ClientAddr.sin_port));
        //删除这个客户端socket from ClientInfoList
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
            //不应执行到此处
            TRACE(_T("_WorkThreadFunc中的m_Optype出错"));
            break;
        }
      }
    }
  }
  TRACE(_T("第 %d 号工作线程退出.\n"), nThreadID);
  //释放线程参数
  if (lpParam != NULL) delete lpParam;
  lpParam = NULL;
  return 0;
}
