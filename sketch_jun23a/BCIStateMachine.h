#ifndef BCI_STATE_MACHINE_H
#define BCI_STATE_MACHINE_H

#include <Arduino.h>
#include <vector>
#include <cstring>

// --- Структура шаблона паттерна ---
struct PatternTemplate {
    const char* name;
    std::vector<bool> signal; // Последовательность битов (например: 1,0,1,0...)
    float currentScore;       // Текущие очки уверенности
    float maxScore;           // Максимум очков (равен длине паттерна)
    bool isActive;            // Флаг: сработал ли паттерн прямо сейчас
    
    PatternTemplate(const char* n, const std::vector<bool>& s) 
        : name(n), signal(s), currentScore(0), maxScore(100), isActive(false) {}
};
	enum {CMD_SET_ACTIVE,CMD_RESET_TEMPLATES,CMD_STATUS,CMD_RECORD};

class BCIStateMachine {
private:
    enum State { ST_IDLE, ST_RECORDING, ST_ANALYZING };
    State currentState;

    // Буфер истории битов (аналог m_bitBuffer в MFC)
    // Хранит последние N бит для сравнения. 512 бит ~ 4 секунды при 125 Гц.
    const int HISTORY_SIZE = 512*10;
    std::vector<bool> bitHistory;

    // Список шаблонов
    std::vector<PatternTemplate> templates;

    // Переменные для записи нового шаблона
    bool isRecordingNewTemplate;
    std::vector<bool> tempRecordingBuffer;
    int recordCount;
    const int MAX_RECORD_LENGTH = 128*5; // Лимит битов для одной записи

    // Результат для отправки в MFC
    int currentBestScore = 0;          // 0-100% лучшего совпадения
    char bestPatternName[32] = {0};    // Имя лучшего паттерна

public:
    BCIStateMachine();
    
    void feedSensorData(int16_t rawValue); // Сюда приходит значение с analogRead
    void processCommand(int cmd);         // Обработка команд от MFC
    void update();                        // Вызывать каждый loop()
    
    // Геттеры для отправки данных в Serial
    int getBestScore() const { return currentBestScore; }
    const char* getBestPatternName() const { return bestPatternName; }

private:
    void addDefaultPatterns();
    void checkBitPatterns();
    void saveCurrentRecording();
    bool convertToBit(int16_t value);    // Конвертация аналогового сигнала в бит
};

#endif
