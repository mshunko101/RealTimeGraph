#include "BCIStateMachine.h"

BCIStateMachine bci;

// НАСТРОЙКИ ТАЙМЕРА
const unsigned long INTERVAL_US = 8475UL; // ~117 Гц (подбирай под свой датчик)
unsigned long previousMicros = 0;




const int SOUND_PIN = 5; // Выбери любой PWM пин (на SAMD21 почти все пины PWM)

// Переменные для генерации тона вручную
unsigned long lastSoundToggleTime = 0;
const unsigned long SOUND_PERIOD_US = 4237UL; // 1000000 / (118 * 2) ~ 4237 мкс
bool soundState = LOW;

bool isSoundActive = true; // Флаг: играть звук или нет





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
  bci.init();
  Serial.println("BCI_SYSTEM_READY_PATTERN_ENGINE");
  
  // Активируем систему
}
bool false_log = true;
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

  // 3. Обрабатываем команды из компьютера (если есть)

    sampleCount++;
  }
  
     if (Serial.available()) {
    bci.processSerialCommands();
  }
 
  // 6. Отладка частоты дискретизации (раз в секунду)
  unsigned long now = millis();
  if (now - lastCheckTime >= 1000) {
    lastCheckTime = now;
   // Serial.print(F("SAMPLES_PER_SEC: "));
   // Serial.println(sampleCount);
    sampleCount = 0;
    false_log = true;

 
  }

tone(5, 118,100); 
}
