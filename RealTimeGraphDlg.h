
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
#include <deque>
#include <algorithm>
#include <map>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
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

struct PatternData {
    CString name;
    std::vector<std::vector<double>> rawRecordings; // Храним все попытки
    std::vector<double> finalTemplate;             // Тут будет усредненный результат
    bool isReady;
};


// Структура шаблона для Pattern Matching
struct PatternTemplate {
    CString name;
    std::deque<bool> signal;
    double threshold = 1.00; // Порог схожести (0.0 - 1.0)
    double currentScore;   // <--- НОВОЕ: текущий процент схожести (0.0 - 1.0)
    double maxScore = 100;     // Максимум очков (например, длина шаблона * 10)
    bool isActive = false;   // Флаг: реально ли сейчас идет этот сигнал
    PatternTemplate(const CString& n, double thresh = 1.0) : name(n), threshold(thresh), currentScore(0), maxScore(100){}
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
    void ProcessBit(bool bitValue);
    // Pattern Matching (Обучение)
    int m_recordCount = 0;
    const int TEMPLATE_LENGTH = 30; // Сколько точек нужно для шаблона
    std::deque<bool> m_bitBuffer;
    std::vector<PatternTemplate> m_templates;
public:
    // Элементы управления (переменные)
    CButton m_btnConnect;
    CButton m_btnStart;
    CButton m_btnRecord;
    CEdit m_editPort;
    CEdit m_editName;
    CStatic m_pStatusText;
    BOOL OnInitDialog() override;
    afx_msg void OnBnClickedBtnConnect();
    afx_msg void OnBnClickedBtnStart();
    afx_msg void OnBnClickedBtnRecord();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
     
public:
    void DrawTemplatePreviews(CDC& dc, const CRect& rect, int margin);
    // Логика работы
    void ParseAndStoreData(const CString& line);
    StatsResult CalculateRegressionAndNormality();
    bool IsAnomalyDetected(const StatsResult& stats);
    void CheckAndActOnPatterns(); // Главная функция поиска паттернов
    // В секции private класса CRealTimeGraphDlg
private:          // Буфер последних битов
    bool m_bRecordingTemplate;             // Флаг записи шаблона
    std::deque<bool> m_tempRecordingBuffer;// Временный буфер для записи

     void CheckBitPatterns();               // Поиск совпадений
    void InitDefaultPatterns();           // Инициализация шаблонов

    // Действия
    void PerformAction(const CString& reason);

    // Вспомогательные
    bool OpenComPort(LPCTSTR portName, HANDLE& hCom);
    void OnBnClickedBtnCalibrate();
    // --- Переменные для калибровки (Юстировка) ---
    bool m_isCalibrating = false;           // Флаг: идет ли сейчас калибровка?
    bool m_isCalibrated = false;            // Флаг: была ли калибровка успешно пройдена?
    double m_baselineOffset = 0.0;         // Найденный "ноль" сенсора
    std::vector<double> m_calibrationBuffer; // Буфер для сбора точек при калибровке

    static const int CALIB_SAMPLES = 150;  // Сколько точек собрать (при Sleep(40) это ~6 сек)
    // Можно уменьшить до 80-100, если 6 секунд долго ждать.
};

