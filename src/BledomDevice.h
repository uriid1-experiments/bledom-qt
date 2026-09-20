#pragma once

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QTimer>
#include <QColor>
#include <QList>

// Controls an ELK-BLEDOM LED strip controller over BLE.
// Protocol: service 0xFFF0, write characteristic 0xFFF3,
// 9-byte commands of the form 7E xx xx xx xx xx xx xx EF.
class BledomDevice : public QObject
{
    Q_OBJECT
public:
    enum class State { Disconnected, Connecting, Discovering, Ready };
    Q_ENUM(State)

    struct Effect {
        quint8 id;
        QString name;
    };

    explicit BledomDevice(QObject *parent = nullptr);
    ~BledomDevice() override;

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    QString deviceName() const { return m_info.name(); }
    QString deviceAddress() const { return m_info.address().toString(); }

    // Built-in effects with localized names (rebuilt on every call so the
    // names follow the current translator).
    static QList<Effect> effects();

public slots:
    // --- scanning ---
    void startScan(int timeoutMs = 8000);
    void stopScan();

    // --- connection ---
    void connectToDevice(const QBluetoothDeviceInfo &info);
    void connectToAddress(const QString &mac);
    void disconnectFromDevice();

    // --- commands ---
    void setPower(bool on);
    void setColor(const QColor &color);
    void setBrightness(int percent);          // 0..100
    void setEffect(quint8 effectId);
    void setEffectSpeed(int percent);         // 0..100
    void syncTime();
    void sendRaw(const QByteArray &packet);   // arbitrary packet (for experiments)

signals:
    void stateChanged(BledomDevice::State state);
    void deviceFound(const QBluetoothDeviceInfo &info);
    void scanFinished();
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);
    void packetSent(const QByteArray &packet);

private:
    static QByteArray makePacket(quint8 b1, quint8 b2, quint8 b3, quint8 b4,
                                 quint8 b5, quint8 b6, quint8 b7);
    void enqueue(const QByteArray &packet);
    void flushQueue();
    void setState(State s);
    void cleanupController();

    void onServiceDiscovered(const QBluetoothUuid &uuid);
    void onDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState s);

    State m_state = State::Disconnected;
    QBluetoothDeviceInfo m_info;

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_service = nullptr;
    QLowEnergyCharacteristic m_writeChar;
    QLowEnergyService::WriteMode m_writeMode = QLowEnergyService::WriteWithoutResponse;

    // Coalescing: while a slider is being dragged only the latest packet of
    // each command type is sent, at most once every ~50 ms. Insertion order
    // is preserved (e.g. "power on" goes out before "set color").
    QTimer m_flushTimer;
    QList<QByteArray> m_pending;
};
