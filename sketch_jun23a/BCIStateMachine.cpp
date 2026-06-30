#include "BCIStateMachine.h"

BCIStateMachine::BCIStateMachine() {
    currentState = ST_IDLE;
    isRecordingNewTemplate = false;
    recordCount = 0;
    currentBestScore = 0;
    bestPatternName[0] = '\0';
    
    addDefaultPatterns();
}

// --- Конвертация аналогового сигнала в бит (0 или 1) ---
bool BCIStateMachine::convertToBit(int16_t value) {

    return value;
}

void BCIStateMachine::addDefaultPatterns() {
    templates.clear();

}

// --- ГЛАВНАЯ ФУНКЦИЯ: Обработка сырых данных ---
void BCIStateMachine::feedSensorData(int16_t rawValue) {
    if (currentState == ST_IDLE) return;

    // 1. Превращаем сырое число в бит
    bool currentBit = convertToBit(rawValue);

    // 2. Добавляем в историю (кольцевой буфер)
    bitHistory.push_back(currentBit);
    if ((int)bitHistory.size() > HISTORY_SIZE) {
        bitHistory.erase(bitHistory.begin());
    }

    // 3. Если режим записи - сохраняем в временный буфер
    if (isRecordingNewTemplate) {
        tempRecordingBuffer.push_back(currentBit);
        recordCount++;
        if (recordCount >= MAX_RECORD_LENGTH) {
            saveCurrentRecording(); // Автосохранение по лимиту
        }
    }

    // 4. ЗАПУСК ПРОВЕРКИ ПАТТЕРНОВ
    checkBitPatterns();
}

// --- ЛОГИКА ПОИСКА ПАТТЕРНОВ (Гистерезис как в MFC) ---
void BCIStateMachine::checkBitPatterns() {
    if (bitHistory.empty()) return;

    currentBestScore = 0;
    bestPatternName[0] = '\0';

    for (auto& pat : templates) {
        size_t len = pat.signal.size();
        
        // Если истории меньше, чем длина паттерна, даем штраф (очки тают)
        if ((int)bitHistory.size() < len) {
            pat.currentScore = fmax(0.0f, pat.currentScore - 2.0f); 
            continue;
        }

        // Считаем совпадение последних N бит
        size_t start = bitHistory.size() - len;
        int matches = 0;
        for (size_t i = 0; i < len; ++i) {
            if (bitHistory[start + i] == pat.signal[i]) {
                matches++;
            }
        }
        float ratio = (float)matches / len;

        // ЛОГИКА ГИТЕРЕЗИСА (Защита от шума)
        if (ratio > 0.61f) {
            // Очень похоже -> ДАЕМ МНОГО ОЧКОВ
            pat.currentScore += 15.0f;
        } else if (ratio < 0.2f) {
            // Совсем не похоже -> ОТНИМАЕМ ОЧКИ
            pat.currentScore -= 10.0f;
        } else {
            // Средне -> чуть снижаем, чтобы случайное совпадение не накопилось
            pat.currentScore -= 2.0f;
        }

        // Ограничиваем очки рамками [0, maxScore]
        if (pat.currentScore < 0) pat.currentScore = 0;
        if (pat.currentScore > pat.maxScore) pat.currentScore = pat.maxScore;

        // --- ПРИНЯТИЕ РЕШЕНИЯ ---
        
        // Проверяем, сработало ли именно сейчас (переход порога 90%)
        bool justTriggered = (pat.currentScore >= pat.maxScore * 0.9f) && !pat.isActive;
        
        if (justTriggered) {
            Serial.print("PATTERN DETECTED: "); // Отладка в порт
            Serial.println(pat.name); // Отладка в порт
            pat.isActive = true;
            
            // Обновляем глобальный лучший результат
            int scorePercent = (int)(pat.currentScore / pat.maxScore * 100.0f);
            if (scorePercent > currentBestScore) {
                currentBestScore = scorePercent;
                strncpy(bestPatternName, pat.name, sizeof(bestPatternName) - 1);
            }
        }

        // Сброс флага активности, если очки упали ниже 30%
        if (pat.currentScore < pat.maxScore * 0.3f) {
            pat.isActive = false;
        }
        
        // Также обновляем лучший результат, даже если не было триггера,
        // чтобы MFC видел "насколько мы близки"
        int currentPercent = (int)(pat.currentScore / pat.maxScore * 100.0f);
        if (currentPercent > currentBestScore) {
            currentBestScore = currentPercent;
            strncpy(bestPatternName, pat.name, sizeof(bestPatternName) - 1);
        }
    }
}

void BCIStateMachine::saveCurrentRecording() {
    if (tempRecordingBuffer.empty()) return;

    char name[32];
    snprintf(name, sizeof(name), "Template_%d", (int)templates.size());
    
    templates.push_back(PatternTemplate(name, tempRecordingBuffer));
    
    tempRecordingBuffer.clear();
    recordCount = 0;
    isRecordingNewTemplate = false; // Останавливаем запись
    
    Serial.print("New Template Saved: ");
    Serial.println(name);
}

void BCIStateMachine::processCommand(int cmd) {
    switch (cmd) {
        case CMD_STATUS:
            break;
            
        case CMD_RECORD:
            isRecordingNewTemplate = !isRecordingNewTemplate; // Переключатель вкл/выкл
            tempRecordingBuffer.clear();
            recordCount = 0;
            if (isRecordingNewTemplate) {
                Serial.println(F("Recording started..."));
            } else {
                saveCurrentRecording();
                Serial.println(F("Recording stopped and saved."));
            }
            break;

        case CMD_RESET_TEMPLATES:
            templates.clear();
            addDefaultPatterns();
            bitHistory.clear();
            Serial.println(F("Templates reset to defaults."));
            break;

        case CMD_SET_ACTIVE:
            currentState = ST_ANALYZING;
            bitHistory.clear(); // Очищаем историю при старте анализа
            Serial.println(F("Analysis mode activated."));
            break;
            
        default:
            break;
    }
}

void BCIStateMachine::update() {
    // Здесь можно добавить дополнительную логику, если нужно
}
