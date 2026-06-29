// RealTimeGraphDlg.cpp: файл реализации
// Адаптировано для работы с битовым потоком (0/1) и поиском битовых паттернов

#include "pch.h"
#include "framework.h"
#include "RealTimeGraph.h"
#include "RealTimeGraphDlg.h"
#include "afxdialogex.h"
#include <commctrl.h>
#include <thread>
#include <deque>
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#undef min
#undef max

BEGIN_MESSAGE_MAP(CRealTimeGraphDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_RECORD, &CRealTimeGraphDlg::OnBnClickedBtnRecord)
    ON_WM_TIMER()
    ON_WM_PAINT()
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(CRealTimeGraphDlg, CDialogEx)

CRealTimeGraphDlg::CRealTimeGraphDlg(CWnd* pParent)
    : CDialogEx(IDD_REALTIMEGRAPH_DIALOG, pParent) {

    m_dataBuffer.reserve(BUFFER_SIZE);
    m_bitBuffer.clear();
    m_isCalibrating = false; // Для битов калибровка часто не нужна, но оставлена для совместимости
    m_isCalibrated = true;   // Считаем, что биты уже готовы к приему
}

CRealTimeGraphDlg::~CRealTimeGraphDlg() {
    if (m_hComPort != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hComPort);
    }
}

void CRealTimeGraphDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_BTN_RECORD, m_btnRecord);
    DDX_Control(pDX, IDC_STATIC_STATUS, m_pStatusText);
    DDX_Control(pDX, IDC_EDIT_NAME, m_editName);
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

ULONG __stdcall ReadThreadProc(LPVOID pParam) {
    CRealTimeGraphDlg* pDlg = reinterpret_cast<CRealTimeGraphDlg*>(pParam);
    if (!pDlg || !pDlg->m_bConnected) return 1;

    char buffer[256];
    DWORD bytesRead = 0;
    DWORD written = 0;

    while (pDlg->m_bConnected && pDlg->m_hComPort != INVALID_HANDLE_VALUE) {
        // Протокол: MFC шлет "SEND_DATA", Arduino отвечает строкой битов ("0", "1" или "10101")
        const char* req = "SEND_DATA\n";
        WriteFile(pDlg->m_hComPort, req, strlen(req), &written, NULL);

        if (ReadFile(pDlg->m_hComPort, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                CString line = CA2W(buffer);
                line.Replace(_T("\r"), _T(""));
                line.Replace(_T("\n"), _T(""));

                if (line.IsEmpty()) {
                    Sleep(5); // Небольшая пауза, если пусто
                    continue;
                }

                // --- ГЛАВНАЯ ЛОГИКА: Обработка битовой строки ---

                // ПАРСИНГ БИТ:
                // Вариант 1: Arduino шлет "0" или "1"
                // Вариант 2: Arduino шлет строку "10101010"

                bool hasData = false;
                for (int i = 0; i < line.GetLength(); ++i) {
                    TCHAR ch = line[i];
                    if (ch == _T('1')) {
                        pDlg->ProcessBit(true);
                        hasData = true;
                    }
                    else if (ch == _T('0')) {
                        pDlg->ProcessBit(false);
                        hasData = true;
                    }
                    // Игнорируем любые другие символы (OK, запятые и т.д.)
                }

                if (!hasData) {
                    // Если строка не содержала 0 или 1, возможно это служебное сообщение
                    // Можно добавить логирование сюда
                    AfxMessageBox(_T("НЕТ ДАННЫХ!"));
                }
            }
        }
        // Уменьшено с 40мс до 5мс для надежного захвата битов при 118bps
        Sleep(5);
    }
    return 0;
}

// --- Обработка отдельного бита ---
void CRealTimeGraphDlg::ProcessBit(bool bitValue) {
    // 1. Добавляем в кольцевой буфер битов
    m_bitBuffer.push_back(bitValue);

    // Ограничиваем историю (например, последние 512 бит)
    const int MAX_BIT_HISTORY = 512;
    if ((int)m_bitBuffer.size() > MAX_BIT_HISTORY) {
        m_bitBuffer.pop_front();
    }

    // 2. Обновляем буфер для графика (конвертируем бит в double 0.0/1.0)
    double voltsForGraph = bitValue ? 1.0 : 0.0;
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        DataPoint pt;
        pt.timeSec = GetTickCount() / 1000.0;
        pt.valueVolts = voltsForGraph;
        m_dataBuffer.push_back(pt);

        if (m_dataBuffer.size() > BUFFER_SIZE) {
            m_dataBuffer.erase(m_dataBuffer.begin());
        }
    }
    if (m_bRecordingTemplate) {
        m_tempRecordingBuffer.push_back(bitValue);
        m_recordCount++;
    }
    // 3. Проверка паттернов (выполняется каждый раз при получении нового бита)
    CheckBitPatterns();
    const int MAX_RECORD_LENGTH = 128;
    if (m_recordCount >= MAX_RECORD_LENGTH) {
        // Автоматически останавливаем запись, если набрали лимит
        m_bRecordingTemplate = false;
         
        if (!m_tempRecordingBuffer.empty()) {
            CString name;
            m_editName.GetWindowText(name);
            if (name.IsEmpty()) name = _T("Auto_Pattern");

            PatternTemplate newPat(name);
            newPat.signal = m_tempRecordingBuffer; // Сохраняем вектор bool
            newPat.threshold = 1.0; // Для битов совпадение должно быть полным

            m_templates.push_back(newPat);

            if (m_pStatusText) {
                CString msg;
                msg.Format(_T("Шаблон '%s' сохранен (%d бит). Всего шаблонов: %d"),
                    name, (int)newPat.signal.size(), (int)m_templates.size());
                m_pStatusText.SetWindowText(msg);
            }

            m_tempRecordingBuffer.clear();
            m_recordCount = 0;

             
        }
        else {
            if (m_pStatusText) m_pStatusText.SetWindowText(_T("Запись отменена (нет данных)"));
        }


        if (m_bRecordingTemplate) {
            // Завершаем запись 
            m_btnRecord.SetWindowText(_T("Начать запись шаблона"));
        }
        else { 
            m_btnRecord.SetWindowText(_T("Идет запись..."));
            if (m_pStatusText) m_pStatusText.SetWindowText(_T("РЕЖИМ ЗАПИСИ: Сделайте событие сейчас!"));
        }


        // Тут можно сразу вызвать логику сохранения, как в кнопке,
        // или просто ждать, пока пользователь нажмет кнопку, чтобы дать имя.
        // Самый простой вариант: просто выключаем флаг, а сохранение делаем по кнопке.

        if (m_pStatusText)
            m_pStatusText.SetWindowText(_T("Лимит битов достигнут. Нажмите кнопку для сохранения."));
    }
    // Запрос перерисовки графика
    InvalidateRect(NULL, FALSE);
}

// --- Поиск битовых паттернов ---
    void CRealTimeGraphDlg::CheckBitPatterns() {
        if (m_bitBuffer.empty()) return;

        for (auto& pat : m_templates) {
            size_t len = pat.signal.size();
            if (len > m_bitBuffer.size()) {
                pat.currentScore = std::max(0.0, pat.currentScore - 2); // Если данных мало, очки тают (шум уходит)
                continue;
            }

            // 1. Считаем совпадение последних N бит
            size_t start = m_bitBuffer.size() - len;
            int matches = 0;
            for (size_t i = 0; i < len; ++i) {
                if (m_bitBuffer[start + i] == pat.signal[i]) matches++;
            }
            double ratio = (double)matches / len;

            // 2. Логика начисления очков (Гистерезис)
            if (ratio > 0.61) {
                // Очень похоже -> ДАЕМ МНОГО ОЧКОВ
                pat.currentScore += 15;
            }
            else if (ratio < 0.2) {
                // Совсем не похоже -> ОТНИМАЕМ ОЧКИ (это точно шум)
                pat.currentScore -= 10;
            }
            else {
                // Средне -> чуть снижаем, чтобы случайное совпадение не накопилось
                pat.currentScore -= 2;
            }

            // Ограничиваем очки рамками [0, maxScore]
            pat.currentScore = std::clamp(pat.currentScore, 0.0, pat.maxScore);

            // 3. ПРИНЯТИЕ РЕШЕНИЯ
            // Срабатываем, только если набрали 90% очков И раньше не срабатывали
            if (pat.currentScore >= pat.maxScore * 0.9 && !pat.isActive) {
                PerformAction(pat.name); // <-- ВОТ ЗДЕСЬ МЫ ПОНЯЛИ, ЧТО ЭТО ДАННЫЕ, А НЕ ШУМ
                pat.isActive = true;
                TRACE(_T("Обнаружен ПАТТЕРН %s!\n"), pat.name);
            }

            // Если очки упали ниже 30% от максимума -> сбрасываем флаг активности
            if (pat.currentScore < pat.maxScore * 0.3) {
                pat.isActive = false;
            }
        }
    }

// --- Действие при срабатывании ---
void CRealTimeGraphDlg::PerformAction(const CString& reason) {
    MessageBeep(MB_ICONEXCLAMATION);

    CFile file;
    UINT openFlags = CFile::modeCreate | CFile::modeWrite | CFile::modeNoTruncate;

    if (file.Open(_T("bit_alerts.log"), openFlags)) {
        CString logEntry;
        CTime now = CTime::GetCurrentTime();
        logEntry.Format(_T("[%s] ТРЕВОГА: %s\r\n"),
            now.Format("%Y-%m-%d %H:%M:%S"),
            reason);
        file.SeekToEnd();
        file.Write(logEntry, logEntry.GetLength() * 2); // Unicode
        file.Close();
    }

    // Отправка команды обратно на Arduino
    if (m_bConnected && m_hComPort != INVALID_HANDLE_VALUE) {
        const char* cmd = "ALARM\n";
        DWORD written = 0;
        WriteFile(m_hComPort, cmd, strlen(cmd), &written, NULL);
    }

    if (m_pStatusText) {
        CString msg;
        msg.Format(_T("СРАБОТАЛО: %s"), reason);
        m_pStatusText.SetWindowText(msg);
    }
}

// --- Обработчики кнопок ---

void CRealTimeGraphDlg::OnBnClickedBtnConnect() {
    CString portName;
    portName.Insert(0, _T("COM6")); // Замени на авто-поиск или ввод пользователя

    if (OpenComPort(portName, m_hComPort)) {
        m_bConnected = true;
        // Запускаем поток чтения
        CreateThread(NULL, 0, ReadThreadProc, this, 0, NULL);

        if (m_pStatusText)
            m_pStatusText.SetWindowText(_T("Порт открыт. Ожидание данных..."));
    }
    else {
        AfxMessageBox(_T("Не удалось открыть порт! Проверьте номер COM-порта."));
    }
}

void CRealTimeGraphDlg::OnBnClickedBtnStart() {
    // Таймер для перерисовки и статистики (если нужна)
    SetTimer(1, 50, NULL);
}

 
// --- Таймер ---
void CRealTimeGraphDlg::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == 1) {
        // Здесь можно добавить легкую статистику, если нужно
        // Например, подсчет частоты переключения битов

        // Перерисовка уже вызывается в ProcessBit, но можно форсировать здесь
         InvalidateRect(NULL, FALSE); 
    }
    CDialogEx::OnTimer(nIDEvent);
}

// --- Отрисовка графика ---
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

    // Подписи
    dc.TextOut(margin - 20, rect.bottom - margin - 15, _T("0"));
    dc.TextOut(margin - 30, margin, _T("1"));

    std::lock_guard<std::mutex> lock(m_bufferMutex);
    if (m_dataBuffer.empty()) {
        dc.TextOut(margin, rect.bottom / 2, _T("Нет данных"));
        return;
    }

    // Для битового графика мин/макс всегда 0 и 1 (или чуть больше для запаса)
    double minV = -0.1;
    double maxV = 1.1;
    double range = maxV - minV;

    CPen penGreen(PS_SOLID, 2, RGB(0, 150, 0));
    dc.SelectObject(&penGreen);

    CPoint prevPoint;
    bool first = true;

    for (size_t i = 0; i < m_dataBuffer.size(); ++i) {
        double t = m_dataBuffer[i].timeSec;
        double v = m_dataBuffer[i].valueVolts;

        // Расчет X: время относительно начала буфера
        double totalTime = m_dataBuffer.back().timeSec - m_dataBuffer.front().timeSec;
        if (totalTime < 0.001) totalTime = 0.001; // Защита от деления на ноль

        int x = margin + static_cast<int>(((t - m_dataBuffer.front().timeSec) / totalTime) * width);
        // Обрезаем X, чтобы не вылезало за границы при резких скачках времени
        if (x < margin) x = margin;
        if (x > rect.right - margin) x = rect.right - margin;

        // Расчет Y: инвертируем, так как в GDI Y растет вниз
        int y = rect.bottom - margin - static_cast<int>(((v - minV) / range) * height);

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

    // Визуализация сохраненных шаблонов (маленькими копиями справа)
    CPen penRed(PS_DASH, 1, RGB(200, 50, 50));
    dc.SelectObject(&penRed);

    int startX = rect.right - margin - 60;
    int startY = rect.bottom - margin - 40;
    int stepX = 4;
    int plotHeight = 30;

    for (const auto& temp : m_templates) {
        // Отрисовка миниатюры шаблона (красная пунктирная линия)
        int tempX = startX;
        int baseY = startY;

        for (size_t i = 0; i < temp.signal.size(); ++i) {
            // Рисуем столбик для каждого бита
            int h = (temp.signal[i] ? plotHeight : 0);
            CRect r(tempX, baseY - h, tempX + stepX, baseY);

            if (temp.signal[i]) {
                dc.FillSolidRect(&r, RGB(255, 200, 200)); // Светло-красный фон для единицы
                dc.Rectangle(&r); // Контур
            }
            // Ноль просто не рисуем (пустое место), чтобы видеть форму

            tempX += stepX;
            if (tempX > rect.right - margin) break; // Не вылезать за экран
        }

        // Подпись имени шаблона под миниатюрой
        CString nameShort = temp.name;
        if (nameShort.GetLength() > 10) nameShort = nameShort.Left(10) + _T("...");
        dc.TextOut(startX, baseY + 10, nameShort);

        startY -= 25; // Сдвиг вниз для следующего шаблона
        if (startY < margin) break; // Если шаблонов много, не рисуем ниже поля
    }

    // --- ОТРИСОВКА ПРОЦЕНТОВ СХОЖЕСТИ ---
    if (!m_templates.empty()) {
        int textStartX = margin;
        int textY = margin + 10;
        int lineHeight = 20;

        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(RGB(0, 0, 0));

        // Заголовок
        dc.TextOut(textStartX, textY, _T("Схожесть паттернов:"));
        textY += lineHeight;

        for (const auto& temp : m_templates) {
            CString displayText;
            // Форматируем: "Имя: 85%"
            displayText.Format(_T("%s: %d%%"), temp.name, static_cast<int>(temp.currentScore * 100));

            // Цвет текста зависит от процента
            COLORREF color = RGB(0, 100, 0); // Зеленый по умолчанию
            if (temp.currentScore > 0.8) color = RGB(255, 255, 0); // Желтый (близко к срабатыванию)
            if (temp.currentScore >= temp.threshold) color = RGB(255, 50, 50); // Красный (сработало)

            dc.SetTextColor(color);
            dc.TextOut(textStartX, textY, displayText);
            textY += lineHeight;
        }

        dc.SetTextColor(RGB(0, 0, 0)); // Возвращаем черный
    }

}

// --- Логика записи шаблона (кнопка "Начать запись") ---
void CRealTimeGraphDlg::OnBnClickedBtnRecord() {
    if (m_bRecordingTemplate) {
        // Завершаем запись
        m_bRecordingTemplate = false;
        m_btnRecord.SetWindowText(_T("Начать запись шаблона"));

        
    }
    else {
        // Начинаем запись
        m_bRecordingTemplate = true;
        m_tempRecordingBuffer.clear();
        m_recordCount = 0;
        m_btnRecord.SetWindowText(_T("Идет запись..."));
        if (m_pStatusText) m_pStatusText.SetWindowText(_T("РЕЖИМ ЗАПИСИ: Сделайте событие сейчас!"));
    }
}
 
// --- Вспомогательная функция для инициализации тестовых шаблонов (вызови в конструкторе или OnInitDialog) ---
void CRealTimeGraphDlg::InitDefaultPatterns() {
    m_templates.clear();

    // Пример 1: Старт кадра (синхрослово)
    PatternTemplate p1(_T("Старт кадра"));
    p1.signal = { 1, 0, 1, 0, 1, 0, 1, 0 };
    p1.threshold = 1.0;
    m_templates.push_back(p1);

    // Пример 2: Сигнал тревоги
    PatternTemplate p2(_T("Тревога"));
    p2.signal = { 0, 0, 0, 1, 1, 1, 1, 1, 0, 0 };
    p2.threshold = 1.0;
    m_templates.push_back(p2);

    // Можно добавить сколько угодно шаблонов любой длины
}


BOOL CRealTimeGraphDlg::OnInitDialog()
{
    UpdateData(0);
    OnBnClickedBtnConnect();
    OnBnClickedBtnStart();
    return TRUE;
}