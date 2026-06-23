
// RealTimeGraphDlg.h: файл заголовка
//

#pragma once
#include "framework.h"
#include <vector>
#include <mutex>
#include <cmath>
#include <vector>
#include <mutex>
#include <string>
#include <cmath>
#include <algorithm>

// Структура одной точки данных
struct DataPoint {
    double timeSec;
    double valueVolts;
};

// Структура статистики (регрессия)
struct StatsResult {
    bool isValid = false;
    double slope = 0.0;
    double intercept = 0.0;
    double rSquared = 0.0;
    double skewness = 0.0;
    double kurtosis = 0.0;
    double stdDev = 0.0;
};

ULONG __stdcall ReadThreadProc(LPVOID pParam);
// Структура шаблона для Pattern Matching
struct PatternTemplate {
    CString name;
    std::vector<double> signal;
    double threshold = 0.85; // Порог схожести (0.0 - 1.0)

    PatternTemplate(const CString& n, double thresh) : name(n), threshold(thresh) {}
};

class CRealTimeGraphDlg : public CDialogEx
{
public:
    DECLARE_DYNAMIC(CRealTimeGraphDlg)
    CRealTimeGraphDlg(CWnd* pParent = nullptr);
    virtual ~CRealTimeGraphDlg();
    // Данные диалогового окна
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_REALTIMEGRAPH_DIALOG };
#endif
public:
    virtual void DoDataExchange(CDataExchange* pDX) override; // <-- ЭТОГО НЕ ХВАТАЛО
    // COM Port
    HANDLE m_hComPort = INVALID_HANDLE_VALUE;
    bool m_bConnected = false;

    // Буферы и синхронизация
    static const int BUFFER_SIZE = 200;
    std::vector<DataPoint> m_dataBuffer;
    std::mutex m_bufferMutex;
public:
    // Статистика
    StatsResult m_lastStats;

    // Pattern Matching (Обучение)
    bool m_bRecordingTemplate = false;
    int m_recordCount = 0;
    const int TEMPLATE_LENGTH = 30; // Сколько точек нужно для шаблона
    std::vector<double> m_tempRecordingBuffer;
    std::vector<PatternTemplate> m_templates;
public:
    // Элементы управления (переменные)
    CButton m_btnConnect;
    CButton m_btnStart;
    CButton m_btnRecord;
    CEdit m_editPort;
    CEdit m_editName;
    CWnd* m_pStatusText;
    BOOL OnInitDialog() override;
    afx_msg void OnBnClickedBtnConnect();
    afx_msg void OnBnClickedBtnStart();
    afx_msg void OnBnClickedBtnRecord();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
     
public:
    // Логика работы
    void ParseAndStoreData(const CString& line);
    StatsResult CalculateRegressionAndNormality();
    bool IsAnomalyDetected(const StatsResult& stats);
    void CheckAndActOnPatterns(); // Главная функция поиска паттернов

    // Действия
    void PerformAction(const CString& reason);

    // Вспомогательные
    bool OpenComPort(LPCTSTR portName, HANDLE& hCom);
};

