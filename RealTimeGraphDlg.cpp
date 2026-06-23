
// RealTimeGraphDlg.cpp: файл реализации
//

#include "pch.h"
#include "framework.h"
#include "RealTimeGraph.h"
#include "RealTimeGraphDlg.h"
#include "afxdialogex.h"
#include <commctrl.h> // Для работы с COM портом
#include <thread>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#undef min
#undef max

// Диалоговое окно CRealTimeGraphDlg


#include "pch.h"
#include <commctrl.h>
#include <fstream>

BEGIN_MESSAGE_MAP(CRealTimeGraphDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RECORD, &CRealTimeGraphDlg::OnBnClickedBtnRecord)
    ON_WM_TIMER()
    ON_WM_PAINT()
END_MESSAGE_MAP()

CRealTimeGraphDlg::CRealTimeGraphDlg(CWnd* pParent) : CDialogEx(IDD_REALTIMEGRAPH_DIALOG, pParent) {
    m_dataBuffer.reserve(BUFFER_SIZE);
}
IMPLEMENT_DYNAMIC(CRealTimeGraphDlg, CDialogEx)
CRealTimeGraphDlg::~CRealTimeGraphDlg() {
    if (m_hComPort != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hComPort);
    }
}
void CRealTimeGraphDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_BTN_RECORD, m_btnRecord);

    DDX_Control(pDX, IDC_EDIT_NAME, m_editName);      // Поле ввода имени шаблона

    // Для статуса (CWnd*) нельзя использовать DDX_Control напрямую в старом стиле,
    // поэтому мы получим его через GetDlgItem в OnInitDialog. 
    // Но если ты хочешь сделать и для него DDX, нужен специальный макрос, 
    // проще оставить получение через GetDlgItem (см. Шаг 3).
}
// --- Инициализация портов ---
bool CRealTimeGraphDlg::OpenComPort(LPCTSTR portName, HANDLE& hCom) {
    hCom = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hCom == INVALID_HANDLE_VALUE) return false;

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(dcb);
    GetCommState(hCom, &dcb);
    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(hCom, &dcb);

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    SetCommTimeouts(hCom, &timeouts);
    return true;
}

// --- Поток чтения (Worker Thread) ---
ULONG __stdcall ReadThreadProc(LPVOID pParam) {
    CRealTimeGraphDlg* pDlg = reinterpret_cast<CRealTimeGraphDlg*>(pParam);
    if (!pDlg || !pDlg->m_bConnected) return 1;

    char buffer[256];
    DWORD bytesRead = 0;
    DWORD written = 0;

    while (pDlg->m_bConnected && pDlg->m_hComPort != INVALID_HANDLE_VALUE) {
        // Протокол: MFC шлет "SEND_DATA", Arduino отвечает строкой
        const char* req = "SEND_DATA\n";
        WriteFile(pDlg->m_hComPort, req, strlen(req), &written, NULL);

        if (ReadFile(pDlg->m_hComPort, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                CString line = CA2W(buffer);
                line.Replace(_T("\r"), _T(""));
                line.Replace(_T("\n"), _T(""));

                // Парсим и сохраняем данные
                pDlg->ParseAndStoreData(line);
            }
        }
        Sleep(40); // Частота опроса ~25 Гц
    }
    return 0;
}

// --- Парсинг строки от Arduino "OK,512,1.650" ---
void CRealTimeGraphDlg::ParseAndStoreData(const CString& line) {
    if (line.Find(_T("OK,")) != 0) return;

    CString rest = line.Mid(3);
    int commaPos = rest.Find(_T(','));
    if (commaPos == -1) return;

    CString strVolt = rest.Mid(commaPos + 1);
    double volts = _tcstod(strVolt, nullptr);

    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);

        DataPoint pt;
        pt.timeSec = GetTickCount() / 1000.0;
        pt.valueVolts = volts;
        m_dataBuffer.push_back(pt);

        // Кольцевой буфер: удаляем старые
        if (m_dataBuffer.size() > BUFFER_SIZE) {
            m_dataBuffer.erase(m_dataBuffer.begin());
        }
    }

    // ЛОГИКА ЗАПИСИ ШАБЛОНА
    if (m_bRecordingTemplate) {
        m_tempRecordingBuffer.push_back(volts);
        m_recordCount++;

        if (m_recordCount >= TEMPLATE_LENGTH) {
            // Автосохранение шаблона
            CString name;
            m_editName.GetWindowText(name);
            if (name.IsEmpty()) name = _T("Auto_Pattern");

            PatternTemplate newPat(name, 0.85);
            newPat.signal = m_tempRecordingBuffer;
            m_templates.push_back(newPat);

            m_bRecordingTemplate = false;
            m_recordCount = 0;
            m_tempRecordingBuffer.clear();

            if (m_pStatusText) {
                CString msg;
                msg.Format(_T("ШАБЛОН '%s' СОХРАНЕН (%d точек). Всего шаблонов: %d"),
                    name, newPat.signal.size(), (int)m_templates.size());
                m_pStatusText->SetWindowText(msg);
            }

            // Переключаем кнопку обратно на "Начать запись"
            m_btnRecord.SetWindowText(_T("Начать запись шаблона"));
        }
        else {
            if (m_pStatusText) {
                CString msg;
                msg.Format(_T("Запись шаблона... собрано %d/%d точек"), m_recordCount, TEMPLATE_LENGTH);
                m_pStatusText->SetWindowText(msg);
            }
        }
    }
    else {
        // Если не запись, просто обновляем статус
        if (m_pStatusText && m_pStatusText->GetWindowTextLength() == 0) {
            m_pStatusText->SetWindowText(_T("Ожидание данных..."));
        }
    }

    InvalidateRect(NULL, FALSE); // Запрос перерисовки
}

// --- МАТЕМАТИКА: Регрессия и Статистика ---
StatsResult CRealTimeGraphDlg::CalculateRegressionAndNormality() {
    StatsResult res;
    res.isValid = false;

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (m_dataBuffer.size() < 10) return res;

    int n = static_cast<int>(m_dataBuffer.size());
    const int WINDOW_SIZE = 50;
    int startIdx = std::max(0, n - WINDOW_SIZE);
    int count = n - startIdx;
    if (count < 10) return res;

    std::vector<double> x(count), y(count);
    for (int i = 0; i < count; ++i) {
        x[i] = m_dataBuffer[startIdx + i].timeSec;
        y[i] = m_dataBuffer[startIdx + i].valueVolts;
    }

    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (int i = 0; i < count; ++i) {
        sumX += x[i]; sumY += y[i];
        sumXY += x[i] * y[i]; sumXX += x[i] * x[i];
    }

    double meanX = sumX / count;
    double meanY = sumY / count;

    double num = sumXY - (sumX * sumY) / count;
    double den = sumXX - (sumX * sumX) / count;

    if (std::abs(den) < 1e-9) { res.slope = 0; res.intercept = meanY; }
    else {
        res.slope = num / den;
        res.intercept = meanY - res.slope * meanX;
    }

    double ssTot = 0, ssRes = 0;
    for (int i = 0; i < count; ++i) {
        double yPred = res.slope * x[i] + res.intercept;
        ssRes += (y[i] - yPred) * (y[i] - yPred);
        ssTot += (y[i] - meanY) * (y[i] - meanY);
    }
    res.rSquared = (ssTot == 0) ? 1.0 : 1.0 - (ssRes / ssTot);

    // Остатки
    std::vector<double> resids(count);
    double sumR = 0;
    for (int i = 0; i < count; ++i) {
        resids[i] = y[i] - (res.slope * x[i] + res.intercept);
        sumR += resids[i];
    }
    double meanR = sumR / count;

    double sqSum = 0;
    for (double r : resids) sqSum += (r - meanR) * (r - meanR);
    res.stdDev = std::sqrt(sqSum / count);

    if (res.stdDev < 1e-6) { res.skewness = 0; res.kurtosis = 3; res.isValid = true; return res; }

    double m3 = 0, m4 = 0;
    for (double r : resids) {
        double z = (r - meanR) / res.stdDev;
        m3 += z * z * z;
        m4 += z * z * z * z;
    }
    res.skewness = m3 / count;
    res.kurtosis = m4 / count;
    res.isValid = true;
    return res;
}

bool CRealTimeGraphDlg::IsAnomalyDetected(const StatsResult& stats) {
    if (!stats.isValid) return false;
    if (stats.rSquared < 0.75) return true;
    if (std::abs(stats.skewness) > 1.2) return true;
    if (stats.kurtosis < 1.5 || stats.kurtosis > 6.0) return true;
    return false;
}

// --- PATTERN MATCHING: Поиск шаблонов ---
double CalculateCorrelation(const std::vector<double>& data, const std::vector<double>& pattern) {
    int n = pattern.size();
    if ((int)data.size() < n) return 0.0;

    double meanD = 0, meanP = 0;
    for (int i = 0; i < n; ++i) {
        meanD += data[data.size() - n + i];
        meanP += pattern[i];
    }
    meanD /= n; meanP /= n;

    double num = 0, d1 = 0, d2 = 0;
    for (int i = 0; i < n; ++i) {
        double d = data[data.size() - n + i] - meanD;
        double p = pattern[i] - meanP;
        num += d * p;
        d1 += d * d;
        d2 += p * p;
    }
    if (d1 == 0 || d2 == 0) return 0.0;
    return num / (std::sqrt(d1) * std::sqrt(d2));
}

// --- Проверка шаблонов и выполнение действий ---
void CRealTimeGraphDlg::CheckAndActOnPatterns() {
    if (m_templates.empty()) return;

    std::lock_guard<std::mutex> lock(m_bufferMutex);

    // Собираем только значения вольт в плоский вектор для корреляции
    std::vector<double> currentSignal;
    currentSignal.reserve(m_dataBuffer.size());
    for (const auto& pt : m_dataBuffer) {
        currentSignal.push_back(pt.valueVolts);
    }

    bool foundAny = false;

    for (const auto& temp : m_templates) {
        if ((int)currentSignal.size() < (int)temp.signal.size()) continue;

        double corr = CalculateCorrelation(currentSignal, temp.signal);

        // Если схожесть выше порога -> ПАТТЕРН НАЙДЕН!
        if (corr > temp.threshold) {
            CString msg;
            msg.Format(_T("!!! ПАТТЕРН ОБНАРУЖЕН: %s (Сходство: %.2f)"),
                temp.name, corr);

            if (m_pStatusText) m_pStatusText->SetWindowText(msg);

            // Выполняем действие
            PerformAction(msg);

            foundAny = true;

            // ВАЖНО: Чтобы не срабатывать 100 раз на один импульс,
            // можно добавить флаг "cooldown" или очистить буфер.
            // Сейчас просто выводим сообщение.
        }
    }

    // Если ничего не найдено, можно сбросить статус, если нужно
    if (!foundAny && m_pStatusText && m_pStatusText->GetWindowTextLength() > 0) {
        // Опционально: раскомментируй, если хочешь видеть "Норма" всегда
        // m_pStatusText->SetWindowText(_T("Норма")); 
    }
}

// --- Действие при срабатывании (сюда вставляй свою логику) ---
void CRealTimeGraphDlg::PerformAction(const CString& reason) {
    // 1. Звуковой сигнал (системный)
    MessageBeep(MB_ICONEXCLAMATION);

    // 2. Логирование в файл
    CFile file;
    // ВАЖНО: modeCreate + modeWrite + modeNoTruncate = APPEND
    UINT openFlags = CFile::modeCreate | CFile::modeWrite | CFile::modeNoTruncate;

    if (file.Open(_T("sensor_alerts.log"), openFlags)) {
        CString logEntry;
        CTime now = CTime::GetCurrentTime();

        // Формируем строку. В ANSI каждый символ = 1 байт.
        logEntry.Format(_T("[%s] ТРЕВОГА: %s\r\n"),
            now.Format("%Y-%m-%d %H:%M:%S"),
            reason);
        file.SeekToEnd();
        // Пишем ровно столько байт, сколько символов в строке (для ANSI)
        file.Write(logEntry, logEntry.GetLength()*2);

        file.Close();

    }

    // 3. Отправка команды обратно на Arduino (если нужно остановить процесс)
    // Пример: шлём "ALERT" на порт
    if (m_bConnected && m_hComPort != INVALID_HANDLE_VALUE) {
        const char* cmd = "ALERT\n";
        DWORD written = 0;
        WriteFile(m_hComPort, cmd, strlen(cmd), &written, NULL);
    }

    // 4. Визуальный эффект (можно сделать мигание кнопки или цвета, 
    // но проще пока оставить сообщение в статусе, которое мы уже поставили)
}

// --- Обработчики кнопок ---

void CRealTimeGraphDlg::OnBnClickedBtnConnect() {
    CString portName; 
    portName.Insert(0, _T("COM6"));

    if (OpenComPort(portName, m_hComPort)) {
        m_bConnected = true;

        // Запускаем поток чтения
        CreateThread(NULL, 0, ReadThreadProc, this, 0, NULL);
    }
    else {
        AfxMessageBox(_T("Не удалось открыть порт! Проверьте номер COM-порта."));
    }
}

void CRealTimeGraphDlg::OnBnClickedBtnStart() {
    SetTimer(1, 50, NULL); // Таймер каждые 50 мс (20 Гц)
}

void CRealTimeGraphDlg::OnBnClickedBtnRecord() {
    // Переключатель режима записи
    if (m_bRecordingTemplate) {
        // Если уже записываем - отменяем (хотя авто-стоп должен сработать сам)
        m_bRecordingTemplate = false;
        m_tempRecordingBuffer.clear();
        m_recordCount = 0;
        m_btnRecord.SetWindowText(_T("Начать запись шаблона"));
        if (m_pStatusText) m_pStatusText->SetWindowText(_T("Запись отменена"));
    }
    else {
        // Начинаем запись
        m_bRecordingTemplate = true;
        m_tempRecordingBuffer.clear();
        m_recordCount = 0;
        m_btnRecord.SetWindowText(_T("Идет запись..."));
        if (m_pStatusText) m_pStatusText->SetWindowText(_T("РЕЖИМ ЗАПИСИ: Сделайте событие сейчас!"));
    }
}

// --- Таймер: главный цикл обработки ---
void CRealTimeGraphDlg::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == 1) {
        // 1. Считаем статистику (регрессия, R^2, асимметрия)
        m_lastStats = CalculateRegressionAndNormality();

        // 2. Проверка на аномалии (статистический шум)
        if (IsAnomalyDetected(m_lastStats)) {
        //    PerformAction(_T("Статистическая аномалия (R^2 низкий или высокая асимметрия)"));
        }

        // 3. Поиск шаблонов (Pattern Matching)
        CheckAndActOnPatterns();

        // Перерисовка графика
        InvalidateRect(NULL, FALSE);
    }
    CDialogEx::OnTimer(nIDEvent);
}

// --- Отрисовка графика (GDI) ---
void CRealTimeGraphDlg::OnPaint() {
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    int margin = 40;
    int width = rect.Width() - 2 * margin;
    int height = rect.Height() - 2 * margin;

    if (width <= 0 || height <= 0) return;

    dc.FillSolidRect(&rect, RGB(255, 255, 255));

    // Оси
    dc.MoveTo(margin, margin);
    dc.LineTo(margin, rect.bottom - margin);
    dc.MoveTo(margin, rect.bottom - margin);
    dc.LineTo(rect.right - margin, rect.bottom - margin);

    // Подписи осей (упрощенно)
    dc.TextOut(margin - 20, rect.bottom - margin - 15, _T("0"));
    dc.TextOut(margin - 30, margin, _T("Max"));

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (m_dataBuffer.empty()) return;

    // Находим мин/макс для масштабирования
    double minV = m_dataBuffer[0].valueVolts;
    double maxV = m_dataBuffer[0].valueVolts;
    for (const auto& p : m_dataBuffer) {
        if (p.valueVolts < minV) minV = p.valueVolts;
        if (p.valueVolts > maxV) maxV = p.valueVolts;
    }
    // Небольшой запас по вертикали
    double range = maxV - minV;
    if (range < 0.1) range = 1.0;
    minV -= range * 0.1;
    maxV += range * 0.1;

    // Рисуем точки/линии
    CPen penGreen(PS_SOLID, 2, RGB(0, 150, 0));
    dc.SelectObject(&penGreen);

    CPoint prevPoint;
    bool first = true;

    for (size_t i = 0; i < m_dataBuffer.size(); ++i) {
        double t = m_dataBuffer[i].timeSec;
        double v = m_dataBuffer[i].valueVolts;

        // Нормализация координат
        int x = margin + static_cast<int>((t - m_dataBuffer.front().timeSec)) * (width / (m_dataBuffer.back().timeSec - m_dataBuffer.front().timeSec + 0.001));
        int y = rect.bottom - margin - static_cast<int>(((v - minV) / (maxV - minV)) * height);

        CPoint currPoint(x, y);

        if (!first) {
            dc.MoveTo(prevPoint);
            dc.LineTo(currPoint);
        }
        else {
            first = false;
        }
        prevPoint = currPoint;
    }

    // ОТРИСОВКА ШАБЛОНОВ (для наглядности: рисуем сохраненные шаблоны полупрозрачно)
    CPen penRed(PS_DASH, 1, RGB(200, 50, 50));
    dc.SelectObject(&penRed);

    for (const auto& temp : m_templates) {
        // Рисуем шаблон поверх графика (упрощенно: просто ломаная из точек шаблона)
        // Тут нужна дополнительная логика маппинга времени, но для простоты 
        // можно рисовать его в углу или просто знать, что он есть.
        // Для демо нарисуем маленькую копию шаблона справа внизу:
        int startX = rect.right - margin - 80;
        int startY = rect.bottom - margin - 60;

        dc.MoveTo(startX, startY);
        for (size_t i = 0; i < temp.signal.size(); ++i) {
            int px = startX + (i * 3); // Сжимаем по X
            int py = startY - static_cast<int>(((temp.signal[i] - minV) / (maxV - minV + 0.001)) * 40); // Масштабируем по Y
            dc.LineTo(px, py);
            dc.MoveTo(px, py); // Сброс для следующей линии, если бы рисовали отдельно
        }
        // Примечание: отрисовка шаблонов прямо на основном графике требует сложной привязки ко времени.
        // Лучше просто доверять тексту статуса. Этот блок - просто визуальная "фишка".
    }
}


BOOL CRealTimeGraphDlg::OnInitDialog()
{
    OnBnClickedBtnConnect();
    OnBnClickedBtnStart();
    UpdateData(0);
    return true;
}