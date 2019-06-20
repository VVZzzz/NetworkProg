#pragma once

/**
 * IOCP模型核心文件.
 */

#include <MSWSock.h>
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

//缓冲区为8K
#define MAX_BUF_LEN 8192
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 3000
//一个核上开启2个线程用来处理I/O
#define THREADS_PERPROCESSOR 2

//在完成端口上投递的I/O操作类型
typedef enum _OPERATION_TYPE {
  ACCEPT_POSTED,  // accept请求
  SEND_POSTED,    // send请求
  RECV_POSTED,    // recv请求
  NULL_POSTED     // null,表示未初始化
} OPERATION_TYPE;

//单IO数据
typedef struct _PERIODATA {
  OVERLAPPED
      m_Overlapped;  //重叠IO,必须是第一个成员.且每个socket都有自己的overlapped
  SOCKET m_Sock;              // Socket
  WSABUF m_Wsabuf;            // WSABUF缓冲区
  char m_szbuf[MAX_BUF_LEN];  // WSABUF中buf成员具体指针指向
  OPERATION_TYPE m_Optype;    //具体操作类型

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

//单Socket数据
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
  CIOCP* m_iocp;    // iocp类指针,调用函数
  int m_nThreadID;  //线程ID
} WORKTHREADPARAMS, *LPWORKTHREADPARAMS;

class CIOCP {
 public:
  CIOCP();
  ~CIOCP();

 public:
  //启动服务器
  bool Start();
  //停止服务器
  void Stop();
  //加载Sock环境
  bool LoadSocketLib();
  //卸载Sock环境
  void UnloadSocketLib() { WSACleanup(); }
  //设置窗口指针
  void SetPMainDlg(CDialogEx* pDlg) { m_pMainDlg = pDlg; }
  //设置端口
  void SetPort(int nport) { m_nPort = nport; }
  //获取IP地址
  CString GetLocalIP() const { return m_strServerIP; }

 private:
  void _CleanUP();
  void _ShowMessage(const CString strFmt, ...) const;
  bool _InitIOCompletionPort();

 private:
 private:
  int m_nPort;            //端口
  int m_nThreads;         //开启线程数
  CString m_strServerIP;  //服务器端IP
  CDialogEx* m_pMainDlg;  //主界面指针

  HANDLE* m_phThreads;         //线程句柄数组
  HANDLE m_hShutDownEvent;     //退出事件
  HANDLE m_hIOCompletionPort;  //完成端口对象
};
