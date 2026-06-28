/*
 * Скетч: Измерение напряжения на пине A1 (для Vidor 4000 и совместимых)
 * 
 * Особенности:
 * - Усреднение 30 замеров для подавления шума.
 * - Расчет напряжения: U = ADC * (3.3 / 1024).
 * - Вывод в Serial: "ADC: [значение], Voltage: [вольт]".
 * 
 * Подключение:
 * - Сигнал подать на пин A1.
 * - Второй контакт датчика/источника на GND.
 */

const int sensorPin = A2;

// --- НАСТРОЙКИ ПОД НОВЫЙ СЕНСОР ---
const float alpha = 0.03;       // Скорость адаптации нуля
const float threshold_on = 0.5; // ПОРОГ: СНИЖЕН! Твои данные теперь ~30-40, так что 15 - это хороший старт.
                                 // Если будет много ложных срабатываний - поставь 20. Если не срабатывает - поставь 10.
const unsigned long lockoutTime = 10; // Блокировка: 500 мс тишины после импульса

float adaptiveZero = 0.0;
unsigned long lastTriggerTime = 0; 

void setup() {
  // Инициализация последовательного порта (9600 бод - стандарт, можно увеличить до 115200)
  Serial.begin(9600);
  
  while (!Serial) {
    ; // Ждем подключения Serial (актуально для некоторых плат при первом запуске)
  }
  
  // Небольшая задержка для стабилизации питания перед первым замером
  delay(100);
}
void loop() 

{
  
  

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



if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "SEND_DATA") {
      Serial.print("OK,");
  // Отправляем данные в формате CSV: время,значение
 
    Serial.print(now);
    Serial.print(",");   
    Serial.println(shouldOutputOne ? 1 : 0);
 
  //Serial.println(voltage, 3); // 3 знака после запятой
    }
    else {
      Serial.println("ERROR: Unknown command");
    }
  // Если нужно видеть подробный лог, раскомментируйте блок ниже и закомментируйте строку выше:
  /*
  Serial.print("Raw ADC (avg): ");
  Serial.print(adcValue);
  Serial.print(" | Напряжение: ");
  Serial.print(voltage, 3);
  Serial.println(" В");
  */
// Интервал между замерами (мс). Не ставьте слишком мало, если используете медленные порты
}
}
 
