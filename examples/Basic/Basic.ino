#include <Arduino.h>
#include <TrickiController.h>
#include <Preferences.h>

TrickiController triki;
Preferences prefs;

enum State { IDLE, SCANNING, CONNECTING, WAIT_STREAM, STREAMING };
State state = IDLE;
uint32_t stateSince = 0;
uint32_t frameCount = 0;
String savedMAC = "";

void setState(State s) {
    if (state != s) {
        Serial.print("["); Serial.print(state); Serial.print("->"); Serial.print(s);
        Serial.print("] h="); Serial.println(ESP.getFreeHeap());
        state = s;
        stateSince = millis();
    }
}

void setup() {
    delay(500);
    Serial.begin(115200);
    delay(500);
    prefs.begin("tricki", false);
    savedMAC = prefs.getString("mac", "");
    Serial.print("Saved MAC: "); Serial.println(savedMAC.length() ? savedMAC : "(none)");

    triki.begin("ESP32 Triki");

    triki.onConnect([]() {
        Serial.println("[EVT] Connected");
    });

    triki.onDisconnect([]() {
        Serial.println("[EVT] Disconnected");
    });

    triki.onData([](const TrickiFrame& f) {
        frameCount++;
        if (frameCount % 5 == 0) {
            Serial.print("G:"); Serial.print(f.gyroX / TrickiController::GYRO_SCALE);
            Serial.print(","); Serial.print(f.gyroY / TrickiController::GYRO_SCALE);
            Serial.print(","); Serial.println(f.gyroZ / TrickiController::GYRO_SCALE);
        }
    });

    triki.onButton([](bool pressed) {
        Serial.print("BTN:"); Serial.println(pressed);
    });

    triki.onScanResult([](const TrickiDevice& d) {
        if (d.name.length() > 0) {
            Serial.print("  [SCAN] "); Serial.print(d.name.c_str());
            Serial.print(" rssi="); Serial.println(d.rssi);
        }
    });

    setState(IDLE);
}

void loop() {
    triki.update();
    uint32_t now = millis();

    switch (state) {

        case IDLE:
            if (now - stateSince > 1000) {
                // Try saved MAC first
                if (savedMAC.length() > 0) {
                    Serial.print("Reconnecting to saved: "); Serial.println(savedMAC);
                    NimBLEAddress addr(savedMAC.c_str(), 1); // type 1 = random
                    if (triki.connect(addr)) {
                        Serial.println("Connected via saved MAC!");
                        setState(CONNECTING);
                        break;
                    }
                    Serial.println("Saved MAC failed, scanning...");
                }

                Serial.println("Scanning for Triki (6s)...");
                triki.startScan(6000);
                setState(SCANNING);
            }
            break;

        case SCANNING:
            if (!triki.isScanning()) {
                auto results = triki.getScanResults();
                Serial.print("Found "); Serial.print(results.size()); Serial.println(" devices");

                bool found = false;
                for (auto& d : results) {
                    String n(d.name.c_str());
                    n.toLowerCase();
                    if (n.indexOf("triki") >= 0) {
                        // Save MAC for next boot
                        savedMAC = d.address.c_str();
                        prefs.putString("mac", savedMAC);
                        Serial.print("Saved new MAC: "); Serial.println(savedMAC);
                        Serial.print("Connecting to "); Serial.println(d.name.c_str());

                        if (triki.connect(d.nimAddress)) {
                            found = true;
                            setState(CONNECTING);
                            break;
                        }
                    }
                }
                if (!found) {
                    Serial.println("No Triki found, retrying...");
                    setState(IDLE);
                }
            }
            break;

        case CONNECTING:
            if (triki.isConnected()) {
                setState(CONNECTING); // reset timer
                if (now - stateSince > 2000) {
                    Serial.print("Starting stream... ");
                    if (triki.startStream()) {
                        Serial.println("OK");
                        setState(WAIT_STREAM);
                    } else {
                        Serial.println("FAILED");
                        setState(IDLE);
                    }
                }
            } else if (now - stateSince > 15000) {
                Serial.println("Connect timeout");
                setState(IDLE);
            }
            break;

        case WAIT_STREAM:
            if (!triki.isConnected()) { setState(IDLE); break; }
            if (frameCount > 0) {
                Serial.println("Receiving data!");
                setState(STREAMING);
            } else if (now - stateSince > 5000) {
                Serial.println("No data"); setState(IDLE);
            }
            break;

        case STREAMING:
            if (!triki.isConnected()) { setState(IDLE); }
            break;
    }

    delay(1);
}

