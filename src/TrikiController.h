#ifndef Triki_CONTROLLER_H
#define Triki_CONTROLLER_H

#include <NimBLEDevice.h>
#include <functional>
#include <vector>

#define NUS_SERVICE_UUID    "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_RX_UUID         "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NUS_TX_UUID         "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define Triki_CMD_START    {0x20, 0x10, 0x00, 0xD0, 0x07, 0x68, 0x00, 0x03}
#define Triki_CMD_STOP     {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}


struct TrikiFrame {
    int16_t gyroX, gyroY, gyroZ;
    int16_t accelX, accelY, accelZ;
    bool    button;
    uint32_t timestamp;
};


struct TrikiDevice {
    std::string name;
    std::string address;
    int         rssi;
    NimBLEAddress nimAddress;
};


class TrikiController {
public:
    using DataCallback       = std::function<void(const TrikiFrame&)>;
    using ButtonCallback     = std::function<void(bool pressed)>;
    using ConnectionCallback = std::function<void()>;
    using ScanCallback       = std::function<void(const TrikiDevice&)>;

    TrikiController();
    ~TrikiController();

    bool begin(const char* deviceName = "ESP32-Triki");
    void end();
    void update();

    bool connect(const NimBLEAddress& address);
    bool connectToAddress(const NimBLEAddress& address);       // reconnect to bonded
    bool connectWithBonding(const NimBLEAddress& address);     // first-time bonding connect
    bool connectToFirst();
    bool connectToAny(uint32_t scanMs = 5000);
    void disconnect();
    bool isConnected() const;

    NimBLEAddress getBondedAddress() const;  // returns first bonded device address (empty if none)

    bool startStream();
    bool stopStream();

    bool startScan(uint32_t durationMs = 5000);
    bool isScanning() const;
    std::vector<TrikiDevice> getScanResults() const;

    void onData(DataCallback cb);
    void onButton(ButtonCallback cb);
    void onConnect(ConnectionCallback cb);
    void onDisconnect(ConnectionCallback cb);
    void onScanResult(ScanCallback cb);

    TrikiFrame getLatestFrame() const;
    bool        hasNewFrame();

    static bool isTrikiDevice(const NimBLEAdvertisedDevice* device);
    std::string connectedAddress() const;

    static constexpr float GYRO_SCALE  = 14.286f; // LSB/(deg/s) LSM6DSL ±2000 dps (70 mdps/LSB)
    static constexpr float ACCEL_SCALE = 2048.0f; // LSB/g LSM6DSL ±16 g
    static constexpr int   DISCARD_FRAMES = 20;   // first N frames after start are garbage

    bool setLED(bool on);  // green LED on characteristic 0x0004

private:
    class ClientCallbacks : public NimBLEClientCallbacks {
    public:
        ClientCallbacks(TrikiController* p) : m_parent(p) {}
        void onConnect(NimBLEClient* c) override;
        void onDisconnect(NimBLEClient* c, int r) override;
    private:
        TrikiController* m_parent;
    };

    class ScanCallbacks : public NimBLEScanCallbacks {
    public:
        ScanCallbacks(TrikiController* p) : m_parent(p) {}
        void onResult(const NimBLEAdvertisedDevice* d) override;
        void onScanEnd(const NimBLEScanResults& r, int reason) override;
    private:
        TrikiController* m_parent;
    };

    friend class ClientCallbacks;
    friend class ScanCallbacks;

    void handleNotification(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len);

    NimBLEClient*               m_pClient   = nullptr;
    NimBLERemoteCharacteristic* m_pRxChar   = nullptr;
    NimBLERemoteCharacteristic* m_pTxChar   = nullptr;
    NimBLEScan*                 m_pScan     = nullptr;

    NimBLEAddress               m_firstTrikiAddr{NimBLEAddress{}};
    bool                        m_haveTrikiAddr = false;

    ClientCallbacks* m_pClientCb = nullptr;
    ScanCallbacks*   m_pScanCb   = nullptr;

    bool m_connected = false;
    bool m_streaming = false;
    bool m_scanning  = false;
    int  m_discardRemaining = DISCARD_FRAMES;

    TrikiFrame   m_latestFrame;
    volatile bool m_newFrame   = false;
    bool          m_lastButton = false;

    DataCallback       m_onData       = nullptr;
    ButtonCallback     m_onButton     = nullptr;
    ConnectionCallback m_onConnect    = nullptr;
    ConnectionCallback m_onDisconnect = nullptr;
    ScanCallback       m_onScanResult = nullptr;

    std::vector<TrikiDevice> m_scanResults;
};

#endif
