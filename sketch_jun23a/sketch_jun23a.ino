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

const int sensorPin = A1;           // Пин для измерения
const int numReadings = 30;         // Количество замеров для усреднения
const float VREF = 3.3;             // Опорное напряжение (для Vidor 4000 это 3.3В)
const int DIVISOR = 1024;           // 2^10 бит = 1024 уровня квантования

void setup() {
  // Инициализация последовательного порта (9600 бод - стандарт, можно увеличить до 115200)
  Serial.begin(9600);
  
  while (!Serial) {
    ; // Ждем подключения Serial (актуально для некоторых плат при первом запуске)
  }

  Serial.println("=== Старт измерений ===");
  Serial.print("Опорное напряжение: ");
  Serial.print(VREF);
  Serial.println(" В");
  Serial.print("Количество точек усреднения: ");
  Serial.println(numReadings);
  Serial.println("-----------------------");
  
  // Небольшая задержка для стабилизации питания перед первым замером
  delay(100);
}
float readVoltageSmoothed() ;
void loop() {
  float voltage = readVoltageSmoothed();
  int adcValue = (int)(voltage * DIVISOR / VREF); // Пересчет обратно в ADC для наглядности

  // Вывод данных в одну строку (удобно для Serial Plotter в Arduino IDE)
  // Формат: "ADC_value Voltage_value"
  //Serial.print(adcValue);
   unsigned long currentTime = millis();
if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "SEND_DATA") {
      Serial.print("OK,");
  // Отправляем данные в формате CSV: время,значение
  Serial.print(currentTime);
  Serial.print(",");
  Serial.println(adcValue);
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

/**
 * Функция считывания и усреднения напряжения.
 * Возвращает напряжение в Вольтах.
 */
float readVoltageSmoothed() {
  long sum = 0;
  
  for (int i = 0; i < numReadings; i++) {
    int rawValue = analogRead(sensorPin);
    sum += rawValue;
    
    // Небольшая пауза между замерами помогает АЦП корректно перезаряжать внутренний конденсатор
    delay(2); 
  }
  
  float averageAdc = (float)sum / numReadings;
  
  // Формула перевода: Значение * (Опорное / Кол-во уровней)
  float voltage = averageAdc * (VREF / DIVISOR);
  
  return voltage;
}
