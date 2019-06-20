#pragma once

/**
 * IOCPģ�ͺ����ļ�.
 */

#include <MSWSock.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

//������Ϊ8K
#define MAX_BUF_LEN 8192
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 3000
//һ�����Ͽ���2���߳���������I/O
#define THREADS_PERPROCESSOR 2

//����ɶ˿���Ͷ�ݵ�I/O��������
typedef enum _OPERATION_TYPE {
  ACCEPT_POSTED,  // accept����
  SEND_POSTED,    // send����
  RECV_POSTED,    // recv����
  NULL_POSTED     // null,��ʾδ��ʼ��
} OPERATION_TYPE;

//��IO����
typedef struct _PERIODATA {
  OVERLAPPED
      m_Overlapped;  //�ص�IO,�����ǵ�һ����Ա.��ÿ��socket�����Լ���overlapped
  SOCKET m_Sock;              // Socket
  WSABUF m_Wsabuf;            // WSABUF������
  char m_szbuf[MAX_BUF_LEN];  // WSABUF��buf��Ա����ָ��ָ��
  OPERATION_TYPE m_Optype;    //�����������

  _PERIODATA() {
    memset(&m_Overlapped, 0, sizeof(m_Overlapped));
    memset(m_szbuf, 0, MAX_BUF_LEN);
    m_Sock = INVALID_SOCKET;
    m_Wsabuf.buf = m_szbuf;
    m_Wsabuf.len = MAX_BUF_LEN;
    m_Optype = NULL_POSTED;
  }
  ~_PERIODATA() {
    if (m_Sock != INVALID_SOCKET) {
      closesocket(m_Sock);
      m_Sock = INVALID_SOCKET;
    }
  }
} PERIODATA, *LPPERIODATA;

//��Socket����
typedef struct _PERSOCKDATA {
  SOCKET m_Sock;
  struct sockaddr_in m_ClientAddr;
  _PERSOCKDATA() {
    m_Sock = INVALID_SOCKET;
    memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));
  }
  ~_PERSOCKDATA() {
    if (m_Sock != INVALID_SOCKET) {
      closesocket(m_Sock);
      m_Sock = INVALID_SOCKET;
    }
  }
} PERSOCKDATA, *LPPERSOCKDATA;
class CIOCP;

typedef struct _WorkThreadParams {
  CIOCP* m_iocp;    // iocp��ָ��,���ú���
  int m_nThreadID;  //�߳�ID
} WORKTHREADPARAMS, *LPWORKTHREADPARAMS;

class CIOCP {
 public:
  CIOCP();
  ~CIOCP();

 public:
  //����������
  bool Start();
  //ֹͣ������
  void Stop();
  //����Sock����
  bool LoadSocketLib();
  //ж��Sock����
  void UnloadSocketLib() { WSACleanup(); }
  //���ô���ָ��
  void SetPMainDlg(CDialogEx* pDlg) { m_pMainDlg = pDlg; }
  //���ö˿�
  void SetPort(int nport) { m_nPort = nport; }
  //��ȡIP��ַ
  CString GetLocalIP() const { return m_strServerIP; }

 private:
  void _CleanUP();
  void _ShowMessage(const CString strFmt, ...) const;
  bool _InitIOCompletionPort();

 private:
 private:
  int m_nPort;            //�˿�
  int m_nThreads;         //�����߳���
  CString m_strServerIP;  //��������IP
  CDialogEx* m_pMainDlg;  //������ָ��

  HANDLE* m_phThreads;         //�߳̾������
  HANDLE m_hShutDownEvent;     //�˳��¼�
  HANDLE m_hIOCompletionPort;  //��ɶ˿ڶ���
};
