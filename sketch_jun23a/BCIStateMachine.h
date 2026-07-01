 
#ifndef BCI_STATE_MACHINE_H
#define BCI_STATE_MACHINE_H

#include <Arduino.h>
#include <vector>
#include <cstring>

struct PatternTemplate {
    const char* name;
    
    // БЫЛО: std::vector<std::vector<bool>> signals;
    // СТАЛО: Один вектор вероятностей (float). 
    // Значение 0.0..1.0 показывает вероятность того, что бит равен 1.
    std::vector<float> centroid; 
    
    int exampleCount; // Сколько раз мы усредняли этот шаблон
    
    float currentScore;
    float maxScore;
    bool isActive;
    
    PatternTemplate(const char* n) 
        : name(n), currentScore(0), maxScore(100), isActive(false), exampleCount(0) {}
        
    // Метод добавления нового примера с пересчетом среднего
    void addExample(const std::vector<bool>& example) {
        if (example.empty()) return;

        // Если это первый пример, просто копируем его как float (1.0 или 0.0)
        if (centroid.empty()) {
            centroid.resize(example.size());
            for (size_t i = 0; i < example.size(); ++i) {
                centroid[i] = (float)example[i];
            }
            exampleCount = 1;
            //maxScore = (float)example.size(); // Длина паттерна
            return;
        }

        // Если примеры уже есть, нужно привести новый пример к длине центроида
        // Вариант А: Обрезать новый пример до длины центроида (если он длиннее)
        // Вариант Б: Расширить центроид (сложнее, лучше фиксировать длину первого примера)
        size_t len = centroid.size();
        if (example.size() < len) return; // Игнорируем слишком короткие примеры

        // Формула скользящего среднего: NewAvg = OldAvg + (NewValue - OldAvg) / N
        // Но проще: Сумма всех значений / Количество. 
        // Чтобы не хранить сумму, сделаем так:
        // 1. Умножаем текущий центроид на старое количество
        // 2. Добавляем новый пример
        // 3. Делим на новое количество
        
        float oldCount = (float)exampleCount;
        float newCount = oldCount + 1.0f;

        for (size_t i = 0; i < len; ++i) {
            float newValue = (float)example[i]; // 1.0 или 0.0
            // Пересчет среднего арифметического
            centroid[i] = (centroid[i] * oldCount + newValue) / newCount;
        }
        
        exampleCount++;
        // maxScore остается равным длине первого примера (или средней длине, если хочешь усложнить)
    }
};

enum {CMD_SET_ACTIVE,CMD_RESET_TEMPLATES,CMD_STATUS,CMD_RECORD};
class BCIStateMachine {
private:
    enum State { ST_IDLE, ST_RECORDING, ST_ANALYZING };
    State currentState;

    const int HISTORY_SIZE = 118 * 10;
    std::vector<bool> bitHistory;
    std::vector<PatternTemplate> templates;

    bool isRecordingNewTemplate;
    std::vector<bool> tempRecordingBuffer;
    int recordCount;
    const int MAX_RECORD_LENGTH = 118 * 1;

    int targetTemplateIndex; // -1 = новый, иначе индекс для усреднения

    int currentBestScore = 0;
    char bestPatternName[32] = {0};

public:
    BCIStateMachine();
    void feedSensorData(int16_t rawValue);
    void processCommand(int cmd);
    void setTargetTemplate(int index);
    int getTemplatesCount() const { return (int)templates.size(); }
    void update();
    int getBestScore() const { return currentBestScore; }
    const char* getBestPatternName() const { return bestPatternName; }

private:
    void addDefaultPatterns();
    void checkBitPatterns();
    void saveCurrentRecording();
    bool convertToBit(int16_t value);
};

#endif
