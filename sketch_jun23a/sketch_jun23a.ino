#include "BCIStateMachine.h"

BCIStateMachine bci;

// НАСТРОЙКИ ТАЙМЕРА
const unsigned long INTERVAL_US = 8550UL; // ~117 Гц (подбирай под свой датчик)
unsigned long previousMicros = 0;

// Счетчик для отладки частоты
volatile uint32_t sampleCount = 0;
unsigned long lastCheckTime = 0;
const int piezoPin = 5; // выберите любой цифровой пин, совместимый с tone()
const int sensorPin = A2;

// --- НАСТРОЙКИ ПОД НОВЫЙ СЕНСОР ---
const float alpha = 0.03;       // Скорость адаптации нуля
const float threshold_on = 0.5; // ПОРОГ: СНИЖЕН! Твои данные теперь ~30-40, так что 15 - это хороший старт.
                                 // Если будет много ложных срабатываний - поставь 20. Если не срабатывает - поставь 10.
const unsigned long lockoutTime = 10; // Блокировка: 500 мс тишины после импульса

float adaptiveZero = 0.0;
unsigned long lastTriggerTime = 0; 

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println("BCI_SYSTEM_READY_PATTERN_ENGINE");
  
  // Активируем систему
  bci.processCommand(CMD_SET_ACTIVE); 
}

void loop() {
  unsigned long currentMicros = micros();

  // 1. ЧЕТКИЙ ТАЙМЕР (Гарантирует частоту дискретизации)
  if (currentMicros - previousMicros >= INTERVAL_US) {
    previousMicros = currentMicros;
    

   // 1. Читаем датчик
  int rawValue = analogRead(sensorPin);
  float currentVal = (float)rawValue;

  // 2. Считаем "Дзен" (адаптивный ноль)
  adaptiveZero = adaptiveZero * (1.0 - alpha) + currentVal * alpha;
  
  // Считаем отклонение
  float deviation = currentVal - adaptiveZero;
  float absDev = abs(deviation);

  unsigned long now = millis();
  bool shouldOutputOne = false;

  // 3. ЛОГИКА: Одиночный импульс
  // Если прошло время блокировки И отклонение больше порога
  if ((now - lastTriggerTime >= lockoutTime) && (absDev > threshold_on)) {
    shouldOutputOne = true;
    lastTriggerTime = now; // Запоминаем время, чтобы следующие 500мс выдавать 0
  }


    bci.feedSensorData(shouldOutputOne);
    sampleCount++;
  }

  // 2. БЫСТРАЯ ОБРАБОТКА КОМАНД (Без String, чтобы не ломать таймер)
  static char buffer[32];
  static int bufIndex = 0;
  
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      buffer[bufIndex] = '\0';
      bufIndex = 0;
      
      if (strcmp(buffer, "STATUS") == 0) bci.processCommand(CMD_STATUS);
      else if (strcmp(buffer, "RECORD") == 0) bci.processCommand(CMD_RECORD);
      else if (strcmp(buffer, "RESET") == 0) bci.processCommand(CMD_RESET_TEMPLATES);
      else if (strcmp(buffer, "START_ANALYSIS") == 0) bci.processCommand(CMD_SET_ACTIVE);
      
    } else {
      if (bufIndex < 31) buffer[bufIndex++] = c;
    }
  }
 
  // 4. ОТЛАДКА ЧАСТОТЫ (Раз в секунду в монитор порта)
  unsigned long now = millis();
  if (now - lastCheckTime >= 1000) {
    lastCheckTime = now;
    Serial.print(F("SAMPLES_PER_SEC: "));
    Serial.println(sampleCount);
    sampleCount = 0;

    Serial.print(F("PATTERN:"));
    Serial.print(bci.getBestPatternName());
    Serial.print(':');
    Serial.println(bci.getBestScore());
  }
}
