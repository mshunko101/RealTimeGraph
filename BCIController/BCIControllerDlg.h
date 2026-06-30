#pragma once
#include "afxwin.h"
#include <vector>
#include <string>

class CBCIControllerDlg : public CDialogEx
{
public:
    CBCIControllerDlg(CWnd* pParent = nullptr);
    ~CBCIControllerDlg();

    enum { IDD = IDD_BCICONTROLLER_DIALOG };

    // Переменные для привязки контролов (DDX)
    CComboBox m_comboPort;
    CButton m_btnConnect;
    CEdit m_editTarget;
    CProgressCtrl m_progressScore;
    CListCtrl m_listLog;

    CStatic m_staticStatus;
    CStatic m_staticPattern;
    CStatic m_staticScore;

    // Данные состояния
    HANDLE m_hComPort = INVALID_HANDLE_VALUE;
    bool m_bConnected = false;
    int m_nCurrentTarget = 0;

    static const UINT_PTR TIMER_ID_POLL = 1;

protected:
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
    void DoDataExchange(CDataExchange* pDX) override;
public:
    afx_msg void OnBnClickedBtnConnect();
    afx_msg void OnBnClickedBtnRecord();
    afx_msg void OnBnClickedBtnSetTarget();
    afx_msg void OnBnClickedBtnReset();
    afx_msg void OnBnClickedBtnClear();

    void OnTimer(UINT_PTR nIDEvent);

private:
    bool OpenComPort(const CString& portName);
    void CloseComPort();
    void SendCommand(const CStringA& cmd);
    void ParseIncomingData(const CStringA& line);
    void AddLogEntry(const CString& msg);
};
