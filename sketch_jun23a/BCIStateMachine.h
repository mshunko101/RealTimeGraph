#pragma once
#include <vector>
#include <cmath>

// ================= НАСТРОЙКИ =================
const int WINDOW_SIZE = 118;      // Полное окно анализа (1 сек)
const int CHUNK_SIZE = 5;         // Группа по 5 бит
const int MAX_TEMPLATES = 5;      
const int SCORE_THRESHOLD = 85;   // Порог срабатывания
const int MIN_DETECTION_TIME = 118*3; // Минимум бит (примерно 0.5 сек), чтобы считать это движением, а не шумом

// Гистерезис

struct PatternTemplate {
    const char* name;
    int etalone[24];       // Один эталонный паттерн
    bool isSet = false;

    PatternTemplate(const char* n) : name(n) {
        isSet = false;
        for(int i=0; i<24; ++i) etalone[i] = 0;
    }

    // Конвертация битов в слова (5 бит -> число 0-31)
    std::vector<int> convertTo5BitWords(const std::vector<bool>& bits) const {
        std::vector<int> words;
        if ((int)bits.size() < WINDOW_SIZE) return words;

        int count = WINDOW_SIZE / CHUNK_SIZE;
        words.reserve(count);
        
        for (int i = 0; i < count; ++i) {
            int startIdx = i * CHUNK_SIZE;
            int val = 0;
            for (int b = 0; b < CHUNK_SIZE; ++b) {
                val <<= 1;
                if (bits[startIdx + b]) val |= 1;
            }
            words.push_back(val);
        }
        return words;
    }

    void setPattern(const std::vector<bool>& fullBitPattern) {
        if ((int)fullBitPattern.size() < WINDOW_SIZE) return;
        std::vector<int> words = convertTo5BitWords(fullBitPattern);
        for (size_t i = 0; i < words.size(); ++i) {
            etalone[i] = words[i];
        }
        isSet = true;
        Serial.print("PATTERN_SAVED:");
        Serial.println(name);
    }

    // ПРОГРЕССИВНАЯ ПРОВЕРКА
    // Возвращает текущую оценку (0-100) на основе доступной части истории
    int getProgressiveScore(const std::vector<bool>& currentHistory, int availableBits) const {
        if (!isSet) return 0;
        
        // Сколько полных 5-битных слов мы можем собрать из доступных бит?
        int wordsAvailable = availableBits / CHUNK_SIZE;
        if (wordsAvailable == 0) return 0;
        
        // Берем срез истории нужной длины
        std::vector<bool> slice(currentHistory.end() - availableBits, currentHistory.end());
        std::vector<int> currentWords = convertTo5BitWords(slice);
        
        if (currentWords.size() == 0) return 0;

        float totalMatch = 0.0f;
        
        // Сравниваем только ту часть, которая уже пришла
        for (size_t i = 0; i < currentWords.size(); ++i) {
            // Важно: берем эталонное значение для этой же позиции
            if (i >= 24) break; 
            
            int expectedVal = etalone[i]; 
            int currentVal = currentWords[i];
            
            float diff = abs(expectedVal - currentVal);
            float maxDiff = 31.0f; 
            float similarity = 1.0f - (diff / maxDiff);
            if (similarity < 0.0f) similarity = 0.0f;
            
            totalMatch += similarity;
        }

        return (int)((totalMatch / currentWords.size()) * 100.0f);
    }

    void reset() {
        isSet = false;
        for(int i=0; i<24; ++i) etalone[i] = 0;
    }
};

class BCIStateMachine {
private:
    PatternTemplate templates[MAX_TEMPLATES] = {
        PatternTemplate("FORWARD"), 
        PatternTemplate("BACKWARD"), 
        PatternTemplate("RIGHT"), 
        PatternTemplate("LEFT"), 
        PatternTemplate("Custom")
    };

    std::vector<bool> bitHistory; 
    std::vector<bool> tempRecordingBuffer;
    
    bool lastBitState = false; 
    
    int currentTarget = 0;
    bool isRecordingNewTemplate = false;
    
    // Флаги защиты
    bool justFinishedRecording = false;
    int cooldownCounter = 0;
    
    // Переменные для прогрессивного детекта
    int lastTriggerTime = -999; // Время последнего срабатывания (чтобы не спамить)
    int minCooldownMs = 1500;   // Минимальная пауза между срабатываниями (мс)

public:
    void init() {
        bitHistory.clear();
        tempRecordingBuffer.clear();
        currentTarget = 0;
        isRecordingNewTemplate = false;
        lastBitState = false;
        justFinishedRecording = false;
        cooldownCounter = 0;
        lastTriggerTime = -999;
    }

    void feedSensorData(int currentBit) {
        // 1. Гистерезис
       
        bitHistory.push_back(currentBit);
        if ((int)bitHistory.size() > WINDOW_SIZE + MIN_DETECTION_TIME) { // Немного буфера для истории
            bitHistory.erase(bitHistory.begin());
        }

        // Логика записи
        if (isRecordingNewTemplate) {
            tempRecordingBuffer.push_back(currentBit);
            if ((int)tempRecordingBuffer.size() >= WINDOW_SIZE) {
                isRecordingNewTemplate = false;
                templates[currentTarget].setPattern(tempRecordingBuffer);
                justFinishedRecording = true;
                cooldownCounter = WINDOW_SIZE * 3;
                tempRecordingBuffer.clear();
            }
        }

        if (justFinishedRecording) {
            if (cooldownCounter > 0) cooldownCounter--;
            else justFinishedRecording = false;
            return;
        }

        checkProgressiveDetection();
    }

    void processSerialCommands() {
        static String inputString = "";
        while (Serial.available()) {
            char inChar = (char)Serial.read();
            if (inChar == '\n') {
                processCommand(inputString);
                inputString = "";
            } else {
                inputString += inChar;
            }
        }
    }

private:
    void checkProgressiveDetection() {
        if ((int)bitHistory.size() < MIN_DETECTION_TIME) return; // Ждем хотя бы полсекунды

        int bestId = -1;
        int maxScore = 0;
        bool isValidMovement = false;

        // Проходим по всем шаблонам
        for (int i = 0; i < MAX_TEMPLATES; ++i) {
            if (!templates[i].isSet) continue;

            // ГЛАВНОЕ ИЗМЕНЕНИЕ:
            // Мы проверяем схожесть на РАЗНЫХ длинах истории.
            // Начинаем с минимального порога (MIN_DETECTION_TIME) и идем до полного окна.
            // Но для простоты и скорости на Arduino мы просто берем текущую доступную длину,
            // если она больше минимальной.
            
            int availableBits = bitHistory.size();
            // Ограничиваем проверку полным окном, чтобы не лезть в старую историю
            if (availableBits > WINDOW_SIZE) availableBits = WINDOW_SIZE;

            int score = templates[i].getProgressiveScore(bitHistory, availableBits);

            // Отладочный вывод: показывает, как растет уверенность
            // Раскомментируй, если хочешь видеть каждый шаг (может засорить порт)
            // Serial.print("[STEP] "); Serial.print(templates[i].name); Serial.print(": "); Serial.println(score);

            if (score > maxScore) {
                maxScore = score;
                bestId = i;
            }

            // Если хоть один шаблон набрал порог - считаем, что движение обнаружено
            if (score >= SCORE_THRESHOLD) {
                isValidMovement = true;
            }
        }

        // Выводим статистику раз в секунду (упрощенно)
        static int debugTimer = 0;
        debugTimer++;
        if (debugTimer > 118) {
            debugTimer = 0;
           // Serial.print("[PROG] ");
            for(int i=0; i<MAX_TEMPLATES; ++i) {
                 if(templates[i].isSet) {
                     // Быстрый пересчет для вывода
                     int s = templates[i].getProgressiveScore(bitHistory, bitHistory.size());
                     Serial.print(templates[i].name); Serial.print(":"); Serial.print(s); Serial.print(" ");
                 }
            }
            Serial.println("");
        }

        // ЛОГИКА СРАБАТЫВАНИЯ
        unsigned long currentMillis = millis();
        
        // Проверяем, не слишком ли часто срабатывает
        if (isValidMovement && (currentMillis - lastTriggerTime > minCooldownMs)) {
            lastTriggerTime = currentMillis;
            
            // Тут твоя логика действия!
            Serial.print("ACTION_TRIGGERED:");
            Serial.print(templates[bestId].name);
            Serial.print(" (Score:");
            Serial.print(maxScore);
            Serial.println(")");
            
            // Пример: digitalWrite(LED, HIGH); delay(100); digitalWrite(LED, LOW);
        }
    }

    void processCommand(String cmd) {
        cmd.trim();
        if (cmd.startsWith("SET_TARGET:")) {
            int val = cmd.substring(11).toInt();
            if (val >= 0 && val < MAX_TEMPLATES) {
                currentTarget = val;
                Serial.print("TARGET_SET:");
                Serial.println(templates[currentTarget].name);
            }
        } 
        else if (cmd == "RECORD") {
            isRecordingNewTemplate = true;
            tempRecordingBuffer.clear();
            Serial.println("RECORDING_STARTED (Hold 1 sec)");
        }
        else if (cmd == "RESET") {
            for(int i=0; i<MAX_TEMPLATES; ++i) templates[i].reset();
            Serial.println("ALL_TEMPLATES_RESET");
        }
    }
};
