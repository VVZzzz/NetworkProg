#include "stdafx.h"
#include "CIOCP.h"
#include "WSAIOCPMFCDlg.h"

CIOCP::CIOCP()
    : m_nPort(DEFAULT_SERVER_PORT),
      m_nThreads(1),
      m_pMainDlg(NULL),
      m_strServerIP(DEFAULT_SERVER_IP),
      m_hShutDownEvent(NULL),
      m_hIOCompletionPort(NULL) {}

CIOCP::~CIOCP() {}

bool CIOCP::Start() {
  //退出event为手动重置初始状态为no-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step 1. 初始化IOCP对象
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("初始化完成端口对象失败! 错误代码:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("初始化完成端口对象成功!\n"));
  }

  // step 2. 创建支持overlapped的listensocket
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
