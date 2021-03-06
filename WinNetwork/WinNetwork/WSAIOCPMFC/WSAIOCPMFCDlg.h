
// WSAIOCPMFCDlg.h: 头文件
//

#pragma once
#include "stdafx.h"
#include "CIOCP.h"

// CWSAIOCPMFCDlg 对话框
class CWSAIOCPMFCDlg : public CDialogEx {
  // 构造
 public:
  CWSAIOCPMFCDlg(CWnd* pParent = nullptr);  // 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_WSAIOCPMFC_DIALOG };
#endif

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);  // DDX/DDV 支持

  // 实现
 protected:
  HICON m_hIcon;

  // 生成的消息映射函数
  virtual BOOL OnInitDialog();
  afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
  afx_msg void OnPaint();
  afx_msg HCURSOR OnQueryDragIcon();
  //开始监听
  afx_msg void OnBnClickedOk();
  //停止监听
  afx_msg void OnBnClickedStop();
  //退出
  afx_msg void OnBnClickedCancel();
  afx_msg void OnDestroy();
  DECLARE_MESSAGE_MAP()

 public:
  inline void Addinformation(const CString strInfo) {
    CListCtrl* pListCtrl = (CListCtrl *)GetDlgItem(IDC_LIST_INFO);
    pListCtrl->InsertItem(0, strInfo);
  }

 private:
  void Init();

  void InitListCtrl();

 private:
  CIOCP m_CIOCP;
};
