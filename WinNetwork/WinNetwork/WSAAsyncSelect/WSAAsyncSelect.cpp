// WSAAsyncSelect.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <stdio.h>
#include "WSAAsyncSelect.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_LOADSTRING 100

// socket消息,与windows内置消息区分
#define WM_SOCKET WM_USER + 1

//当前在线用户数量
int g_nCount = 0;

// 全局变量:
HINSTANCE hInst;                      // 当前实例
WCHAR szTitle[MAX_LOADSTRING];        // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];  // 主窗口类名

// 此代码模块中包含的函数的前向声明:
SOCKET InitSocket();
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
LRESULT OnSocketEvent(HWND hwnd, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  // TODO: 在此处放置代码。
  SOCKET hListenSock = InitSocket();
  if (hListenSock == INVALID_SOCKET) return -1;

  // 初始化全局字符串
  LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
  LoadStringW(hInstance, IDC_WSAASYNCSELECT, szWindowClass, MAX_LOADSTRING);
  MyRegisterClass(hInstance);

  // 执行应用程序初始化:
  HWND hwnd = InitInstance(hInstance, nCmdShow);
  if (hwnd == NULL) return -1;

  HACCEL hAccelTable =
      LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WSAASYNCSELECT));

  // WSAAsynselect将侦听socket与hwnd绑定在一起,一旦socket有相应事件,那么就会发送消息WM_SOCKET
  // WSAAsynselect在后续版本不建议使用,建议使用重叠I/O模型
  if (WSAAsyncSelect(hListenSock, hwnd, WM_SOCKET, FD_ACCEPT) == -1) {
    printf("WSAAsyncSelect error.");
    return -1;
  }

  //注意将我们定义的WM_SOCKET消息处理写入WndProc函数中

  MSG msg;

  // 主消息循环:
  while (GetMessage(&msg, nullptr, 0, 0)) {
    if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  closesocket(hListenSock);
  return (int)msg.wParam;
}

//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
  WNDCLASSEXW wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WSAASYNCSELECT));
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WSAASYNCSELECT);
  wcex.lpszClassName = szWindowClass;
  wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

  return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
HWND InitInstance(HINSTANCE hInstance, int nCmdShow) {
  hInst = hInstance;  // 将实例句柄存储在全局变量中

  HWND hWnd =
      CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                    0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

  if (!hWnd) {
    return NULL;
  }

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  return hWnd;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//  WM_SOCKET   - 自定义的socket消息,执行处理消息函数
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  switch (message) {
    case WM_COMMAND: {
      int wmId = LOWORD(wParam);
      // 分析菜单选择:
      switch (wmId) {
        case IDM_ABOUT:
          DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
          break;
        case IDM_EXIT:
          DestroyWindow(hWnd);
          break;
        default:
          return DefWindowProc(hWnd, message, wParam, lParam);
      }
    } break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      // TODO: 在此处添加使用 hdc 的任何绘图代码...
      EndPaint(hWnd, &ps);
    } break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_SOCKET:
      return OnSocketEvent(hWnd, wParam, lParam);
    default:
      //没有任何窗口消息提供默认处理
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
  UNREFERENCED_PARAMETER(lParam);
  switch (message) {
    case WM_INITDIALOG:
      return (INT_PTR)TRUE;

    case WM_COMMAND:
      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
        EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
      }
      break;
  }
  return (INT_PTR)FALSE;
}

//初始化Socket函数
SOCKET InitSocket() {
  // 1.初始环境
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("WSAStartup error.");
    WSACleanup();
    return INVALID_SOCKET;
  }

  // 2.服务器地址
  SOCKET hListenSock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addrServ;
  addrServ.sin_family = AF_INET;
  addrServ.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
  addrServ.sin_port = htons(3000);

  // 3.bind
  if (bind(hListenSock, (struct sockaddr*)&addrServ, sizeof(addrServ)) == -1) {
    printf("bind error.");
    WSACleanup();
    return INVALID_SOCKET;
  }
  // 4.listen,开始监听
  if (listen(hListenSock, SOMAXCONN) == -1) {
    printf("listen error.");
    WSACleanup();
    return INVALID_SOCKET;
  }

  return hListenSock;
}

// Socket消息处理函数
// 发到hwnd上的消息(MSG类型),wParam标识网络事件的socket
// lParam高位表示ErrorNode,lParam低位表示已发生的网络事件
LRESULT OnSocketEvent(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  SOCKET s = (SOCKET)wParam;
  int nEventType = WSAGETSELECTEVENT(lParam);
  int nErrorNode = WSAGETSELECTERROR(lParam);
  if (nErrorNode != 0) return 1;
  switch (nEventType) {
    case FD_ACCEPT: {
      //新连接accept
      struct sockaddr_in addrClient;
      int addrClienLen = sizeof(addrClient);
      SOCKET hSockClient =
          accept(s, (struct sockaddr*)&addrClient, &addrClienLen);
      if (hSockClient != -1) {
        //当hSockClient有事件时,会发送WM_SOCKET消息到hwnd窗口,至于什么事件由最后一个参数决定
        if (WSAAsyncSelect(hSockClient, hwnd, WM_SOCKET, FD_READ | FD_CLOSE) ==
            SOCKET_ERROR) {
          closesocket(hSockClient);
          return 1;
        }

        g_nCount++;
        TCHAR szLogMsg[64];
        wsprintf(szLogMsg, _T("a client connected, socket: %d, current %d\n"),
                 (int)hSockClient, g_nCount);
        OutputDebugString(szLogMsg);
      }
    } break;
    case FD_READ: {
      char szBuf[64] = {0};
      int n = recv(s, szBuf, 64, 0);
      if (n > 0)
        OutputDebugStringA(szBuf);
      else if (n <= 0) {
        g_nCount--;
        TCHAR szLogMsg[64];
        wsprintf(szLogMsg,
                 _T("a client disconnected, socket: %d, current: %d\n"), (int)s,
                 g_nCount);
        OutputDebugString(szLogMsg);
        closesocket(s);
      }
    } break;
    case FD_CLOSE:
      g_nCount--;
      TCHAR szLogMsg[64];
      wsprintf(szLogMsg, _T("a client disconnected, socket: %d, current: %d\n"),
               (int)s, g_nCount);
      OutputDebugString(szLogMsg);
      closesocket(s);
      break;
    default:
      break;
  }
}
