# TrikiController — ESP32 BLE library for Żabka Triki

Biblioteka Arduino dla ESP32 do komunikacji z kontrolerem **Triki** (gadżet z Żabki) przez BLE.

Oparta na zreverse-engineeringowanym protokole z projektów:
- [zabka-triki-hardware](https://github.com/moe-takasaki/zabka-triki-hardware) — analiza sprzętowa

## Funkcje

- Skanowanie i automatyczne łączenie z kontrolerem Triki przez BLE
- Odczyt 6-osiowych danych IMU (3 × żyroskop, 3 × akcelerometr) w czasie rzeczywistym (~104 Hz)
- Stan przycisku
- Callbacki i/lub polling
- Stałe do konwersji jednostek (`GYRO_SCALE`, `ACCEL_SCALE`)

## Wymagania

- ESP32 / ESP8266 (testowane na ESP32)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) — lżejsza alternatywa dla oryginalnego stosu BLE

## Instalacja
s
### PlatformIO

```ini
lib_deps =
    h2zero/NimBLE-Arduino@^2.2
    https://github.com/xmonpl/triki-esp32lib.git
```

### Arduino IDE

1. Zainstaluj **NimBLE-Arduino** przez Library Manager
2. Skopiuj folder `triki-esp32lib` do `~/Documents/Arduino/libraries/`

## Szybki start

```cpp
#include <TrikiController.h>

TrikiController triki;

void setup() {
    Serial.begin(115200);
    triki.begin("ESP32-Triki");

    triki.onData([](const TrikiFrame& f) {
        float gyroX = f.gyroX / TrikiController::GYRO_SCALE; // deg/s
        Serial.printf("Gyro: %.1f %.1f %.1f\n", f.gyroX, f.gyroY, f.gyroZ);
    });

    triki.onButton([](bool pressed) {
        Serial.printf("Button: %s\n", pressed ? "pressed" : "released");
    });

    triki.onConnect([]() {
        triki.startStream();
    });

    triki.connectToAny(5000);
}

void loop() {
    triki.update();
    delay(1);
}
```

## Licencja

MIT
