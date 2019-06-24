#pragma once

/**
 * IOCPģ�ͺ����ļ�.
 */

#include <MSWSock.h>
#include <WinSock2.h>
#include <list>
#pragma comment(lib, "ws2_32.lib")

//������Ϊ8K
#define MAX_BUF_LEN 8192
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 3000
//һ�����Ͽ���2���߳���������I/O
#define THREADS_PERPROCESSOR 2
//ͬʱͶ��Accept���������(���������)
#define MAX_POST_ACCEPT 10
//���ݸ������̵߳��˳���־
#define EXIT_CODE NULL

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
  m_Overlapped;				  // �ص�IO,�����ǵ�һ����Ա.��ÿ��socket�����Լ���overlapped
  SOCKET m_Sock;              // Socket
  WSABUF m_Wsabuf;            // WSABUF������
  char m_szbuf[MAX_BUF_LEN];  // WSABUF��buf��Ա����ָ��ָ��
  OPERATION_TYPE m_Optype;    // �����������

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

  //���buffer
  void ResetBuf() {
  memset(m_szbuf, 0, MAX_BUF_LEN);
  }
} PERIODATA, *LPPERIODATA;

//��Socket����
typedef struct _PERSOCKDATA {
  SOCKET m_Sock;
  struct sockaddr_in m_ClientAddr;
  std::list<PERIODATA*> m_ListPerIO;  //�����socket��Ͷ�ݵ�����(��������)

  _PERSOCKDATA() {
    m_Sock = INVALID_SOCKET;
    memset(&m_ClientAddr, 0, sizeof(m_ClientAddr));
  }
  ~_PERSOCKDATA() {
    if (m_Sock != INVALID_SOCKET) {
      closesocket(m_Sock);
      m_Sock = INVALID_SOCKET;
    }
	//�ͷ�����
    for (auto pperiodata : m_ListPerIO) delete pperiodata;
    m_ListPerIO.clear();
  }
  //���һ��������,�����ظ�����
  PERIODATA* GetNewRequest() {
    PERIODATA* newRequest = new PERIODATA;
    m_ListPerIO.emplace_back(newRequest);
    return newRequest;
  }
  //ɾ��ĳ���ض�����
  void RemoveSpecificRequest(PERIODATA* pRequest) {
    for (auto itr = m_ListPerIO.begin(); itr != m_ListPerIO.end(); itr++)
      if (*itr == pRequest) {
        m_ListPerIO.erase(itr);
        break;
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
  CString GetLocalIP();

 private:
  void _CleanUP();
  void _ShowMessage(const CString strFmt, ...) const;
  bool _InitIOCompletionPort();
  bool _InitListenSocket();

  //Ͷ��Accept����
  bool _PostAccept(PERIODATA* pAcceptIOData);
  //Ͷ��Recv����
  bool _PostRecv(PERIODATA* pAcceptIOData);

  //ɾ��ĳ���ͻ���socket������IO����frome ClientListinfo
  void _ClearClientListInfo(PERSOCKDATA* pSockInfo);
  //ɾ�����пͻ���socket������IO����frome ClientListinfo
  void _ClearALLClientListInfo();
  //��ClientListinfo ��ӿͻ���socket��Ϣ
  void _AddClientListInfo(PERSOCKDATA* pSockInfo);

  //����Accept����
  bool _DoAccept(PERSOCKDATA* pSockInfo, PERIODATA* pIOInfo);
  //����Recv����
  bool _DoRecv(PERSOCKDATA* pSockInfo, PERIODATA* pIOInfo);

 private:
  //�̺߳���,ר�Ŵ���IO������߳�
  static DWORD WINAPI _WorkerThreadFunc(LPVOID lpParam);
 private:
  int m_nPort;            //�˿�
  int m_nThreads;         //�����߳���
  CString m_strServerIP;  //��������IP
  CDialogEx* m_pMainDlg;  //������ָ��

  HANDLE* m_phThreads;					//�߳̾������
  HANDLE m_hShutDownEvent;				//�˳��¼�
  HANDLE m_hIOCompletionPort;			//��ɶ˿ڶ���
  LPPERSOCKDATA m_pListenSockinfo;		//����sock��Ϣ

  LPFN_ACCEPTEX m_lpfnAcceptEx;			//AcceptEx����ָ��
  LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockAddrs;

  std::list<PERSOCKDATA*> m_listClientSockinfo;  //�ͻ��˵�sock��Ϣ����(ʼ��Ҫ����ά��,ֻ����ͻ���sock������listensock)
  CRITICAL_SECTION m_csClientInfoList;           //���������Ϣ������ٽ���(������߳̾���)
};
