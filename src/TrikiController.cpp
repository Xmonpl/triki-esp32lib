#include "TrikiController.h"
#include <Arduino.h>
#include <cstring>

static const uint8_t CMD_START[] = Triki_CMD_START;
static const uint8_t CMD_STOP[]  = Triki_CMD_STOP;
static constexpr size_t CMD_START_LEN = sizeof(CMD_START);
static constexpr size_t CMD_STOP_LEN  = sizeof(CMD_STOP);

void TrikiController::ClientCallbacks::onConnect(NimBLEClient* client) {
    m_parent->m_connected = true;
    if (m_parent->m_onConnect) {
        m_parent->m_onConnect();
    }
}

void TrikiController::ClientCallbacks::onDisconnect(NimBLEClient* client, int reason) {
    m_parent->m_connected = false;
    m_parent->m_streaming = false;
    if (m_parent->m_onDisconnect) {
        m_parent->m_onDisconnect();
    }
}

void TrikiController::ScanCallbacks::onResult(const NimBLEAdvertisedDevice* device) {
    TrikiDevice d;
    d.name       = device->getName();
    d.address    = device->getAddress().toString();
    d.rssi       = device->getRSSI();
    d.nimAddress = device->getAddress();

    m_parent->m_scanResults.push_back(d);

    if (m_parent->m_onScanResult) {
        m_parent->m_onScanResult(d);
    }

    if (m_parent->isTrikiDevice(device) && !m_parent->m_haveTrikiAddr) {
        m_parent->m_firstTrikiAddr = device->getAddress();
        m_parent->m_haveTrikiAddr  = true;
    }
}

void TrikiController::ScanCallbacks::onScanEnd(const NimBLEScanResults& results, int reason) {
    m_parent->m_scanning = false;
}


TrikiController::TrikiController() {
    m_pClientCb = new ClientCallbacks(this);
    m_pScanCb   = new ScanCallbacks(this);
    memset(&m_latestFrame, 0, sizeof(m_latestFrame));
}

TrikiController::~TrikiController() {
    end();
    delete m_pClientCb;
    delete m_pScanCb;
}

bool TrikiController::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    return true;
}

void TrikiController::end() {
    disconnect();
    NimBLEDevice::deinit(true);
}

void TrikiController::update() {
    
}


bool TrikiController::connectToAddress(const NimBLEAddress& address) {
    return connect(address);
}

bool TrikiController::connect(const NimBLEAddress& address) {
    if (m_connected) {
        disconnect();
    }

    m_pClient = NimBLEDevice::createClient();
    m_pClient->setClientCallbacks(m_pClientCb, false);
    m_pClient->setConnectTimeout(10);

    if (!m_pClient->connect(address, false, false, true)) {
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        return false;
    }

    NimBLERemoteService* pService = m_pClient->getService(NUS_SERVICE_UUID);
    if (!pService) {
        m_pClient->disconnect();
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        return false;
    }

    delay(200);

    m_pRxChar = pService->getCharacteristic(NUS_RX_UUID);
    m_pTxChar = pService->getCharacteristic(NUS_TX_UUID);

    if (!m_pRxChar || !m_pTxChar) {
        m_pClient->disconnect();
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        m_pRxChar = nullptr;
        m_pTxChar = nullptr;
        return false;
    }

    m_connected = true;
    return true;
}

bool TrikiController::connectToFirst() {
    for (auto& d : m_scanResults) {
        std::string name = d.name;
        for (auto& c : name) c = tolower(c);
        if (name.find("triki") != std::string::npos) {
            return connect(d.nimAddress);
        }
    }
    if (m_haveTrikiAddr) {
        return connect(m_firstTrikiAddr);
    }
    return false;
}

bool TrikiController::connectToAny(uint32_t scanMs) {
    if (!startScan(scanMs)) return false;

    uint32_t started = millis();
    while (isScanning() && (millis() - started < scanMs + 2000)) {
        delay(10);
    }

    if (m_haveTrikiAddr) {
        return connect(m_firstTrikiAddr);
    }

    for (auto& d : m_scanResults) {
        if (connect(d.nimAddress)) return true;
    }
    return false;
}

bool TrikiController::connectWithBonding(const NimBLEAddress& address) {
    if (m_connected) disconnect();

    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    m_pClient = NimBLEDevice::createClient();
    m_pClient->setClientCallbacks(m_pClientCb, false);
    m_pClient->setConnectTimeout(10);

    if (!m_pClient->connect(address, false, false, true)) {
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        return false;
    }

    NimBLERemoteService* pService = m_pClient->getService(NUS_SERVICE_UUID);
    if (!pService) {
        m_pClient->disconnect();
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        return false;
    }

    delay(200);

    m_pRxChar = pService->getCharacteristic(NUS_RX_UUID);
    m_pTxChar = pService->getCharacteristic(NUS_TX_UUID);

    if (!m_pRxChar || !m_pTxChar) {
        m_pClient->disconnect();
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        m_pRxChar = nullptr;
        m_pTxChar = nullptr;
        return false;
    }

    m_connected = true;
    return true;
}

NimBLEAddress TrikiController::getBondedAddress() const {
    try {
        return NimBLEDevice::getBondedAddress(0);
    } catch (...) {
        return NimBLEAddress{};
    }
}

void TrikiController::disconnect() {
    if (m_pClient) {
        if (m_streaming) {
            stopStream();
        }
        if (m_pClient->isConnected()) {
            m_pClient->disconnect();
        }
        NimBLEDevice::deleteClient(m_pClient);
        m_pClient = nullptr;
        m_pRxChar = nullptr;
        m_pTxChar = nullptr;
    }
    m_connected = false;
    m_streaming = false;
}

bool TrikiController::isConnected() const {
    return m_connected && m_pClient && m_pClient->isConnected();
}

bool TrikiController::startStream() {
    if (!m_pClient || !m_pRxChar || !m_pTxChar) {
        log_e("startStream: null members");
        return false;
    }

    if (!m_pTxChar->canNotify()) {
        log_e("startStream: cannot notify");
        return false;
    }

    auto self = this;
    if (!m_pTxChar->subscribe(true,
        [self](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
            self->handleNotification(nullptr, data, len);
        }, false)) {
        log_e("startStream: subscribe failed");
        return false;
    }

    delay(100);

    if (!m_pRxChar->writeValue(CMD_START, CMD_START_LEN, false)) {
        log_e("startStream: write failed");
        m_pTxChar->unsubscribe();
        return false;
    }

    m_streaming = true;
    m_discardRemaining = DISCARD_FRAMES;
    return true;
}

bool TrikiController::stopStream() {
    if (!m_streaming) return false;

    if (m_pRxChar) {
        m_pRxChar->writeValue(CMD_STOP, CMD_STOP_LEN, true);
    }
    if (m_pTxChar) {
        m_pTxChar->unsubscribe();
    }

    m_streaming = false;
    return true;
}

bool TrikiController::startScan(uint32_t durationMs) {
    if (m_scanning) return false;

    m_scanResults.clear();
    m_haveTrikiAddr = false;

    m_pScan = NimBLEDevice::getScan();
    m_pScan->setScanCallbacks(m_pScanCb);
    m_pScan->setActiveScan(true);
    m_pScan->setInterval(100);
    m_pScan->setWindow(99);

    m_scanning = true;
    bool ok = m_pScan->start(durationMs, false);
    if (!ok) {
        m_scanning = false;
    }
    return ok;
}

bool TrikiController::isScanning() const {
    return m_scanning && m_pScan && m_pScan->isScanning();
}

std::vector<TrikiDevice> TrikiController::getScanResults() const {
    return m_scanResults;
}

void TrikiController::handleNotification(
    NimBLERemoteCharacteristic* chr,
    uint8_t* data,
    size_t len)
{
    size_t offset = 0;

    while (offset + 2 <= len) {
        uint8_t type      = data[offset];
        uint8_t buttonRaw = data[offset + 1];
        offset += 2;

        if (type == 0x21) {
            offset += 2;
            continue;
        }

        if (type == 0x22) {
            if (offset + 12 > len) break;

            int16_t gx = (int16_t)(data[offset]     | (data[offset + 1]  << 8));
            int16_t gy = (int16_t)(data[offset + 2] | (data[offset + 3]  << 8));
            int16_t gz = (int16_t)(data[offset + 4] | (data[offset + 5]  << 8));
            int16_t ax = (int16_t)(data[offset + 6] | (data[offset + 7]  << 8));
            int16_t ay = (int16_t)(data[offset + 8] | (data[offset + 9]  << 8));
            int16_t az = (int16_t)(data[offset + 10]| (data[offset + 11] << 8));
            offset += 12;

            bool button = (buttonRaw == 1);

            m_latestFrame.gyroX  = gx;
            m_latestFrame.gyroY  = gy;
            m_latestFrame.gyroZ  = gz;
            m_latestFrame.accelX = ax;
            m_latestFrame.accelY = ay;
            m_latestFrame.accelZ = az;
            m_latestFrame.button = button;
            m_latestFrame.timestamp = millis();
            m_newFrame = true;

            // Discard first N frames after stream start (garbage data)
            if (m_discardRemaining > 0) {
                m_discardRemaining--;
                continue;
            }

            if (m_onData) {
                m_onData(m_latestFrame);
            }

            if (button != m_lastButton) {
                m_lastButton = button;
                if (m_onButton) {
                    m_onButton(button);
                }
            }
        }
    }
}

void TrikiController::onData(DataCallback cb)       { m_onData       = cb; }
void TrikiController::onButton(ButtonCallback cb)   { m_onButton     = cb; }
void TrikiController::onConnect(ConnectionCallback cb)    { m_onConnect    = cb; }
void TrikiController::onDisconnect(ConnectionCallback cb) { m_onDisconnect = cb; }
void TrikiController::onScanResult(ScanCallback cb)       { m_onScanResult = cb; }

TrikiFrame TrikiController::getLatestFrame() const {
    return m_latestFrame;
}

bool TrikiController::hasNewFrame() {
    bool result = m_newFrame;
    m_newFrame = false;
    return result;
}

bool TrikiController::isTrikiDevice(const NimBLEAdvertisedDevice* device) {
    if (!device) return false;

    std::string name = device->getName();
    for (auto& c : name) c = tolower(c);
    if (name.find("triki") != std::string::npos) return true;

    if (device->isAdvertisingService(NimBLEUUID(NUS_SERVICE_UUID))) return true;

    return false;
}

std::string TrikiController::connectedAddress() const {
    if (m_connected && m_pClient) {
        return m_pClient->getPeerAddress().toString();
    }
    return "";
}

bool TrikiController::setLED(bool on) {
    if (!m_connected) return false;
    NimBLERemoteService* pNus = m_pClient->getService(NUS_SERVICE_UUID);
    if (!pNus) return false;
    NimBLERemoteCharacteristic* pCtrl = pNus->getCharacteristic("6e400004-b5a3-f393-e0a9-e50e24dcca9e");
    if (!pCtrl) return false;
    uint8_t val = on ? 0x01 : 0x00;
    return pCtrl->writeValue(&val, 1, false);
}
