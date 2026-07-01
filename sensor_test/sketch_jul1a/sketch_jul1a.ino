const int analogPin = A2;      // Аналоговые пины лучше обозначать как A0–A5
const int fixedResistor = 10000; // 10 кОм

// Целевая частота: 118 Гц -> период ~8.47 мс
const unsigned long sampleInterval = 8; // в миллисекундах (округлённо)

unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200); // Увеличиваем скорость порта для лучшей пропускной способности
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= sampleInterval) {
    previousMillis = currentMillis;

    int rawValue = analogRead(analogPin);
    float voltage = rawValue * (5.0 / 1023.0);

    // Защита от деления на ноль (если напряжение почти 5В)
    if (voltage >= 5.0) {
      voltage = 4.999;
    }

    float rldr = (voltage * fixedResistor) / (5.0 - voltage);

    // Выводим данные не каждый раз, а, например, каждые 50 замеров, чтобы не забивать порт
    
      Serial.println(rldr);
  }
}
