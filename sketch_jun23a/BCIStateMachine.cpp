#include "BCIStateMachine.h"

BCIStateMachine::BCIStateMachine() {
    currentState = ST_IDLE;
    isRecordingNewTemplate = false;
    recordCount = 0;
    targetTemplateIndex = -1;
    currentBestScore = 0;
    bestPatternName[0] = '\0';
    addDefaultPatterns();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);    // Выключаем
}

bool BCIStateMachine::convertToBit(int16_t value) {
    // !!! ВАЖНО: Подбери порог под свою плату !!!
    // AVR (Uno/Nano): 0..1023 -> ставь ~512
    // SAMD21 (Zero): 0..4095 -> ставь ~2048
    const int THRESHOLD = 512; 
    return value;
}

void BCIStateMachine::addDefaultPatterns() {
    templates.clear();

}

void BCIStateMachine::feedSensorData(int16_t rawValue) {
    if (currentState == ST_IDLE) return;

    bool currentBit = convertToBit(rawValue);

    bitHistory.push_back(currentBit);
    if ((int)bitHistory.size() > HISTORY_SIZE) {
        bitHistory.erase(bitHistory.begin());
    }

    if (isRecordingNewTemplate) {
        tempRecordingBuffer.push_back(currentBit);
        recordCount++;
        if (recordCount >= MAX_RECORD_LENGTH) {
            saveCurrentRecording();

        }
    }

    checkBitPatterns();
}

void BCIStateMachine::checkBitPatterns() {
    if (bitHistory.empty()) return;

    currentBestScore = 0;
    bestPatternName[0] = '\0';

    for (auto& pat : templates) {
        if (pat.centroid.empty()) continue;
        
        size_t len = pat.centroid.size();
        if ((int)bitHistory.size() < len) continue;

        size_t start = bitHistory.size() - len;
        
        // --- ЛОГИКА СРАВНЕНИЯ С УСРЕДНЕННЫМ ВЕКТОРОМ ---
        float totalDifference = 0.0f;
        
        for (size_t i = 0; i < len; ++i) {
            float expected = pat.centroid[i];      // Например, 0.7 (скорее 1)
            float actual = (float)bitHistory[start + i]; // 0.0 или 1.0
            
            // Считаем абсолютную разницу. 
            // Если ожидали 0.7 и получили 1.0 -> разница 0.3 (хорошо)
            // Если ожидали 0.7 и получили 0.0 -> разница 0.7 (плохо)
            totalDifference += fabs(expected - actual);
        }
        
        // Нормализуем ошибку. 
        // totalDifference лежит в диапазоне [0, len].
        // Нам нужно превратить это в "уверенность" (Score).
        // Чем меньше разница, тем выше счет.
        float matchRatio = 1.0f - (totalDifference / len); 
        
        // matchRatio теперь от 0.0 (полное несовпадение) до 1.0 (идеальное совпадение)

        // --- ГИТЕРЕЗИС (как и раньше) ---
        if (matchRatio > 0.61f) {
            pat.currentScore += 15.0f;
        } else if (matchRatio < 0.2f) {
            pat.currentScore -= 10.0f;
        } else {
            pat.currentScore -= 2.0f;
        }

        if (pat.currentScore < 0) pat.currentScore = 0;
        if (pat.maxScore > 0 && pat.currentScore > pat.maxScore) pat.currentScore = pat.maxScore;

        bool justTriggered = (pat.maxScore > 0 && 
                               pat.currentScore >= pat.maxScore ) && !pat.isActive;
        
        if (justTriggered) {
            Serial.print(F("PATTERN DETECTED: "));
            Serial.println(pat.name);
            pat.isActive = true;
        }


        int currentPercent = 0;
        if (pat.maxScore > 0) {
            currentPercent = (int)(pat.currentScore / pat.maxScore * 100.0f);
        }
        
        if (currentPercent > currentBestScore) {
            currentBestScore = currentPercent;
            strncpy(bestPatternName, pat.name, sizeof(bestPatternName) - 1);
        }

        if (pat.maxScore > 0 && pat.currentScore < pat.maxScore * 0.3f) {
            pat.isActive = false;
        }
        
    }
}

void BCIStateMachine::saveCurrentRecording() {
    if (tempRecordingBuffer.empty()) return;

    if (targetTemplateIndex != -1) {
        // РЕЖИМ УСРЕДНЕНИЯ: добавляем пример к существующему
        if (targetTemplateIndex >= 0 && targetTemplateIndex < (int)templates.size()) {
            templates[targetTemplateIndex].addExample(tempRecordingBuffer);
            Serial.print(F("Example averaged into template: "));
            Serial.println(templates[targetTemplateIndex].name);
        digitalWrite(LED_BUILTIN, LOW);    // Выключаем
        }
        targetTemplateIndex = -1;
    } else {
        // РЕЖИМ СОЗДАНИЯ НОВОГО
        char* name = new char[32];
        snprintf(name, 32, "Template_%d", (int)templates.size());
        
        PatternTemplate newPat(name);
        newPat.addExample(tempRecordingBuffer);
        templates.push_back(newPat);
        
        Serial.print(F("New Template Created (Averaged): "));
        Serial.println(name);
        digitalWrite(LED_BUILTIN, LOW);    // Выключаем
    }

    tempRecordingBuffer.clear();
    recordCount = 0;
    isRecordingNewTemplate = false;
}

void BCIStateMachine::setTargetTemplate(int index) {
    if (index >= 0 && index < (int)templates.size()) {
        targetTemplateIndex = index;
        Serial.print(F("Target set to template #"));
        Serial.println(index);
    } else {
        Serial.println(F("Invalid target index"));
        targetTemplateIndex = -1;
    }
}

void BCIStateMachine::processCommand(int cmd) {
    switch (cmd) {
        case CMD_RECORD:
            digitalWrite(LED_BUILTIN, HIGH);   // Включаем (все каналы)
            isRecordingNewTemplate = !isRecordingNewTemplate;
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
            Serial.println(F("Templates reset."));
            break;

        case CMD_SET_ACTIVE:
            currentState = ST_ANALYZING;
            bitHistory.clear();
            Serial.println(F("Analysis mode activated."));
            break;
            
        default:
            break;
    }
}

void BCIStateMachine::update() {}
