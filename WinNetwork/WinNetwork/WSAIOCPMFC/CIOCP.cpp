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
  //�˳�eventΪ�ֶ����ó�ʼ״̬Ϊno-signaled
  m_hShutDownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

  // step 1. ��ʼ��IOCP����
  if (false == _InitIOCompletionPort()) {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ʧ��! �������:%d!\n"),
                 WSAGetLastError());
    _CleanUP();
    return false;
  } else {
    _ShowMessage(_T("��ʼ����ɶ˿ڶ���ɹ�!\n"));
  }

  // step 2. ����֧��overlapped��listensocket
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
