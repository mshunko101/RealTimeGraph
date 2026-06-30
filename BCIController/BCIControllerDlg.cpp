#include "pch.h" // Или stdafx.h для старых версий VS
#include "BCIControllerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CBCIControllerDlg, CDialogEx)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BTN_CONNECT, &CBCIControllerDlg::OnBnClickedBtnConnect)
    ON_BN_CLICKED(IDC_BTN_RECORD, &CBCIControllerDlg::OnBnClickedBtnRecord)
    ON_BN_CLICKED(IDC_BTN_SET_TARGET, &CBCIControllerDlg::OnBnClickedBtnSetTarget)
    ON_BN_CLICKED(IDC_BTN_RESET, &CBCIControllerDlg::OnBnClickedBtnReset)
END_MESSAGE_MAP()

CBCIControllerDlg::CBCIControllerDlg(CWnd* pParent)
    : CDialogEx(IDD_BCICONTROLLER_DIALOG, pParent)
{
}

CBCIControllerDlg::~CBCIControllerDlg()
{
    if (m_bConnected) CloseComPort();
}

// --- ГЛАВНОЕ: Привязка переменных к элементам формы ---
void CBCIControllerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_COMBO_PORT, m_comboPort);
    DDX_Control(pDX, IDC_BTN_CONNECT, m_btnConnect);
    DDX_Control(pDX, IDC_EDIT_TARGET, m_editTarget);
    DDX_Control(pDX, IDC_PROGRESS_SCORE, m_progressScore);
    DDX_Control(pDX, IDC_LIST_LOG, m_listLog);

    DDX_Control(pDX, IDC_STATIC_STATUS, m_staticStatus);
    DDX_Control(pDX, IDC_STATIC_PATTERN, m_staticPattern);
    DDX_Control(pDX, IDC_STATIC_SCORE, m_staticScore);
}

BOOL CBCIControllerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Инициализация прогресс-бара
    m_progressScore.SetRange(0, 100);
    m_progressScore.SetPos(0);

    // Настройка списка логов (колонки)
    m_listLog.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 150);
    m_listLog.InsertColumn(1, _T("Message"), LVCFMT_LEFT, 400);

    // Заполняем ComboBox портами COM1-COM16
    for (int i = 1; i <= 16; ++i) {
        CString portName;
        portName.Format(_T("COM%d"), i);

        HANDLE hTest = CreateFile(portName, GENERIC_READ | GENERIC_WRITE,
            0, NULL, OPEN_EXISTING, 0, NULL);
        if (hTest != INVALID_HANDLE_VALUE) {
            CloseHandle(hTest);
            m_comboPort.AddString(portName);
        }
    }

    if (m_comboPort.GetCount() > 0)
        m_comboPort.SetCurSel(0);

    AddLogEntry(_T("System Ready. Select COM port."));
    return TRUE;
}

void CBCIControllerDlg::AddLogEntry(const CString& msg)
{
    CTime t = CTime::GetCurrentTime();
    CString timeStr = t.Format(_T("%H:%M:%S"));

    int nIndex = m_listLog.InsertItem(0, timeStr);
    m_listLog.SetItemText(nIndex, 1, msg);
    m_listLog.EnsureVisible(0, FALSE);
}

bool CBCIControllerDlg::OpenComPort(const CString& portName)
{
    DCB dcb = { 0 };
    COMMTIMEOUTS timeouts;

    m_hComPort = CreateFile(portName, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_hComPort == INVALID_HANDLE_VALUE) return false;

    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_hComPort, &dcb)) { CloseHandle(m_hComPort); return false; }

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(m_hComPort, &dcb)) { CloseHandle(m_hComPort); return false; }

    GetCommTimeouts(m_hComPort, &timeouts);
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    SetCommTimeouts(m_hComPort, &timeouts);

    PurgeComm(m_hComPort, PURGE_TXCLEAR | PURGE_RXCLEAR);

    m_bConnected = true;
    m_staticStatus.SetWindowText(_T("Status: Connected"));
    m_btnConnect.SetWindowText(_T("Disconnect"));
    AddLogEntry(_T("Connected to ") + portName);

    SetTimer(TIMER_ID_POLL, 100, nullptr);

    return true;
}

void CBCIControllerDlg::CloseComPort()
{
    KillTimer(TIMER_ID_POLL);
    if (m_hComPort != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hComPort);
        m_hComPort = INVALID_HANDLE_VALUE;
    }
    m_bConnected = false;
    m_staticStatus.SetWindowText(_T("Status: Disconnected"));
    m_btnConnect.SetWindowText(_T("Connect"));
    AddLogEntry(_T("Port closed."));
}

void CBCIControllerDlg::SendCommand(const CStringA& cmd)
{
    if (!m_bConnected || m_hComPort == INVALID_HANDLE_VALUE) return;

    CStringA fullCmd = cmd + "\r\n";
    DWORD bytesWritten;
    WriteFile(m_hComPort, (LPCSTR)fullCmd, fullCmd.GetLength(), &bytesWritten, NULL);
    AddLogEntry(_T("Sent: ") + CString(CA2W(cmd)));
}

void CBCIControllerDlg::ParseIncomingData(const CStringA& line)
{
    if (line.Find("PATTERN:") == 0) 
    {
        int firstColon = line.Find(':', 8);
        int secondColon = line.Find(':', firstColon + 1);

        if (firstColon != -1 && secondColon != -1) {
            CStringA name = line.Mid(firstColon + 1, secondColon - firstColon - 1);
            CStringA scoreStr = line.Mid(secondColon + 1);

            int score = _ttoi(CString(CA2W(scoreStr)));

            m_staticPattern.SetWindowText(CString(CA2W(name)));
            CString scoreText;
            scoreText.Format(_T("Score: %d%%"), score);
            m_staticScore.SetWindowText(scoreText);
            m_progressScore.SetPos(score);

            if (score > 80) {
                Beep(1000, 50);
            }
        }
    }
    else if (line.Find(("BCI_SYSTEM_READY")) == 0) {
        AddLogEntry(_T("Arduino Booted Successfully"));
    }
    else {
        AddLogEntry(CString(CA2W(line)));
    }
}

void CBCIControllerDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent != TIMER_ID_POLL)
    {
        CDialogEx::OnTimer(nIDEvent);
        return;
    }

    if (!m_bConnected || m_hComPort == INVALID_HANDLE_VALUE)
    {
        CDialogEx::OnTimer(nIDEvent);
        return;
    }

    char buffer[256];
    DWORD bytesRead = 0;
    COMSTAT commStat;
    DWORD dwErrors;

    // 1. Получаем статус порта. Это НЕ блокирующая операция.
    if (!ClearCommError(m_hComPort, &dwErrors, &commStat))
    {
        AddLogEntry(_T("Error getting comm status"));
        CDialogEx::OnTimer(nIDEvent);
        return;
    }

    // 2. Если в буфере нет данных (cbInQue == 0), мы НЕ вызываем ReadFile!
    // Мы просто выходим из функции. Интерфейс остается отзывчивым.
    if (commStat.cbInQue == 0)
    {
        // Опционально: можно раскомментировать для отладки, чтобы видеть "тишину"
        // AddLogEntry(_T("No data in buffer")); 
        CDialogEx::OnTimer(nIDEvent);
        return;
    }

    // 3. ТОЛЬКО если данные есть, читаем их.
    // ReadFile здесь сработает мгновенно, так как данные уже ждут в буфере.
    BOOL bRead = ReadFile(m_hComPort, buffer, sizeof(buffer) - 1, &bytesRead, NULL);

    if (bRead && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        CStringA data((LPCSTR)buffer);

        int pos = 0;
        CStringA oneLine = data.Tokenize("\r\n", pos);
        while (!oneLine.IsEmpty())
        {
            if (!oneLine.IsEmpty())
            {
                ParseIncomingData(oneLine);
            }
            oneLine = data.Tokenize("\r\n", pos);
        }
    }
    else
    {
        // Обработка ошибок чтения, если нужно
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) // Игнорируем ожидающие операции, если вдруг
        {
            // Можно добавить логирование ошибки
        }
    }

    CDialogEx::OnTimer(nIDEvent);
}


void CBCIControllerDlg::OnBnClickedBtnConnect()
{
    if (m_bConnected) {
        CloseComPort();
        return;
    }

    int sel = m_comboPort.GetCurSel();
    if (sel == CB_ERR) return;

    CString portName;
    m_comboPort.GetLBText(sel, portName);

    if (OpenComPort(portName)) {
        Sleep(2000);
        SendCommand("START_ANALYSIS");
    }
    else {
        AfxMessageBox(_T("Failed to open port!"));
    }
}

void CBCIControllerDlg::OnBnClickedBtnRecord()
{
    if (!m_bConnected) return;

    SendCommand("RECORD");
    AddLogEntry(_T("Recording started..."));
}

void CBCIControllerDlg::OnBnClickedBtnSetTarget()
{
    if (!m_bConnected) return;

    CString targetStr;
    m_editTarget.GetWindowText(targetStr);
    int targetIdx = _ttoi(targetStr);

    CStringA cmd;
    cmd.Format("SET_TARGET:%d", targetIdx);
    SendCommand(cmd);

    m_nCurrentTarget = targetIdx;
    AddLogEntry(CString(CA2W(cmd)));
}

void CBCIControllerDlg::OnBnClickedBtnReset()
{
    if (!m_bConnected) return;
    SendCommand("RESET");
    AddLogEntry(_T("Templates reset on Arduino."));
}
