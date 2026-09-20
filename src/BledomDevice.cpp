#include "BledomDevice.h"

#include <QBluetoothAddress>
#include <QBluetoothUuid>
#include <QDateTime>

namespace {
const QBluetoothUuid kServiceUuid(QStringLiteral("0000fff0-0000-1000-8000-00805f9b34fb"));
const QBluetoothUuid kWriteCharUuid(QStringLiteral("0000fff3-0000-1000-8000-00805f9b34fb"));
constexpr int kFlushIntervalMs = 50;
}

BledomDevice::BledomDevice(QObject *parent)
    : QObject(parent)
{
    m_flushTimer.setInterval(kFlushIntervalMs);
    m_flushTimer.setSingleShot(true);
    connect(&m_flushTimer, &QTimer::timeout, this, &BledomDevice::flushQueue);
}

BledomDevice::~BledomDevice()
{
    cleanupController();
}

QList<BledomDevice::Effect> BledomDevice::effects()
{
    return {
        {0x87, tr("Jump: R/G/B")},
        {0x88, tr("Jump: 7 colors")},
        {0x89, tr("Fade: R/G/B")},
        {0x8a, tr("Fade: 7 colors")},
        {0x8b, tr("Fade: red")},
        {0x8c, tr("Fade: green")},
        {0x8d, tr("Fade: blue")},
        {0x8e, tr("Fade: yellow")},
        {0x8f, tr("Fade: cyan")},
        {0x90, tr("Fade: magenta")},
        {0x91, tr("Fade: white")},
        {0x92, tr("Fade: red/green")},
        {0x93, tr("Fade: red/blue")},
        {0x94, tr("Fade: green/blue")},
        {0x95, tr("Blink: 7 colors")},
        {0x96, tr("Blink: red")},
        {0x97, tr("Blink: green")},
        {0x98, tr("Blink: blue")},
        {0x99, tr("Blink: yellow")},
        {0x9a, tr("Blink: cyan")},
        {0x9b, tr("Blink: magenta")},
        {0x9c, tr("Blink: white")},
    };
}

// ---------------------------------------------------------------- scanning

void BledomDevice::startScan(int timeoutMs)
{
    if (m_agent) {
        if (m_agent->isActive())
            return;
        m_agent->deleteLater();
        m_agent = nullptr;
    }

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(timeoutMs);

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this,
            [this](const QBluetoothDeviceInfo &info) {
                if (info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)
                    emit deviceFound(info);
            });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this] {
        emit logMessage(tr("Scan finished"));
        emit scanFinished();
    });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::canceled, this, [this] {
        emit logMessage(tr("Scan cancelled"));
        emit scanFinished();
    });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
            [this](QBluetoothDeviceDiscoveryAgent::Error) {
                emit errorOccurred(tr("Scan error: %1").arg(m_agent->errorString()));
                emit scanFinished();
            });

    emit logMessage(tr("Scanning for BLE devices..."));
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BledomDevice::stopScan()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
}

// ---------------------------------------------------------------- connection

void BledomDevice::connectToAddress(const QString &mac)
{
    QBluetoothDeviceInfo info(QBluetoothAddress(mac), QStringLiteral("ELK-BLEDOM"), 0);
    info.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
    connectToDevice(info);
}

void BledomDevice::connectToDevice(const QBluetoothDeviceInfo &info)
{
    stopScan();
    cleanupController();

    m_info = info;
    emit logMessage(tr("Connecting to %1 (%2)...").arg(info.name(), info.address().toString()));

    m_controller = QLowEnergyController::createCentral(m_info, this);
    m_controller->setRemoteAddressType(QLowEnergyController::PublicAddress);

    connect(m_controller, &QLowEnergyController::connected, this, [this] {
        emit logMessage(tr("Connected, discovering services..."));
        setState(State::Discovering);
        m_controller->discoverServices();
    });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this] {
        emit logMessage(tr("Disconnected"));
        m_pending.clear();
        m_service = nullptr; // deleted together with the controller
        setState(State::Disconnected);
    });
    connect(m_controller, &QLowEnergyController::errorOccurred, this,
            [this](QLowEnergyController::Error) {
                emit errorOccurred(tr("BLE error: %1").arg(m_controller->errorString()));
                if (m_state != State::Ready)
                    setState(State::Disconnected);
            });
    connect(m_controller, &QLowEnergyController::serviceDiscovered, this,
            &BledomDevice::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished, this,
            &BledomDevice::onDiscoveryFinished);

    setState(State::Connecting);
    m_controller->connectToDevice();
}

void BledomDevice::disconnectFromDevice()
{
    if (!m_controller)
        return;
    if (m_controller->state() == QLowEnergyController::UnconnectedState) {
        cleanupController();
        setState(State::Disconnected);
        return;
    }
    m_controller->disconnectFromDevice();
}

void BledomDevice::cleanupController()
{
    m_flushTimer.stop();
    m_pending.clear();
    m_writeChar = QLowEnergyCharacteristic();
    if (m_service) {
        m_service->deleteLater();
        m_service = nullptr;
    }
    if (m_controller) {
        m_controller->disconnect(this);
        if (m_controller->state() != QLowEnergyController::UnconnectedState)
            m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
}

void BledomDevice::onServiceDiscovered(const QBluetoothUuid &uuid)
{
    if (uuid == kServiceUuid)
        emit logMessage(tr("Found service FFF0"));
}

void BledomDevice::onDiscoveryFinished()
{
    if (!m_controller)
        return;

    m_service = m_controller->createServiceObject(kServiceUuid, this);
    if (!m_service) {
        emit errorOccurred(tr("Service FFF0 not found - is this an ELK-BLEDOM?"));
        m_controller->disconnectFromDevice();
        return;
    }

    connect(m_service, &QLowEnergyService::stateChanged, this,
            &BledomDevice::onServiceStateChanged);
    connect(m_service, &QLowEnergyService::errorOccurred, this,
            [this](QLowEnergyService::ServiceError e) {
                emit errorOccurred(tr("Service error: %1").arg(int(e)));
            });
    m_service->discoverDetails();
}

void BledomDevice::onServiceStateChanged(QLowEnergyService::ServiceState s)
{
    if (s != QLowEnergyService::RemoteServiceDiscovered)
        return;

    m_writeChar = m_service->characteristic(kWriteCharUuid);
    if (!m_writeChar.isValid()) {
        emit errorOccurred(tr("Characteristic FFF3 not found"));
        m_controller->disconnectFromDevice();
        return;
    }

    // Prefer write-without-response: it is faster and the strip never
    // acknowledges anyway.
    const auto props = m_writeChar.properties();
    m_writeMode = (props & QLowEnergyCharacteristic::WriteNoResponse)
                      ? QLowEnergyService::WriteWithoutResponse
                      : QLowEnergyService::WriteWithResponse;

    emit logMessage(tr("Ready. Write mode: %1")
                        .arg(m_writeMode == QLowEnergyService::WriteWithoutResponse
                                 ? tr("without response")
                                 : tr("with response")));
    setState(State::Ready);
}

void BledomDevice::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

// ---------------------------------------------------------------- commands

QByteArray BledomDevice::makePacket(quint8 b1, quint8 b2, quint8 b3, quint8 b4,
                                    quint8 b5, quint8 b6, quint8 b7)
{
    QByteArray p;
    p.append(char(0x7e));
    p.append(char(b1));
    p.append(char(b2));
    p.append(char(b3));
    p.append(char(b4));
    p.append(char(b5));
    p.append(char(b6));
    p.append(char(b7));
    p.append(char(0xef));
    return p;
}

void BledomDevice::setPower(bool on)
{
    if (on)
        enqueue(makePacket(0x00, 0x04, 0xf0, 0x00, 0x01, 0xff, 0x00));
    else
        enqueue(makePacket(0x00, 0x04, 0x00, 0x00, 0x00, 0xff, 0x00));
}

void BledomDevice::setColor(const QColor &c)
{
    enqueue(makePacket(0x00, 0x05, 0x03, quint8(c.red()), quint8(c.green()), quint8(c.blue()), 0x00));
}

void BledomDevice::setBrightness(int percent)
{
    percent = qBound(0, percent, 100);
    enqueue(makePacket(0x00, 0x01, quint8(percent), 0x00, 0x00, 0x00, 0x00));
}

void BledomDevice::setEffect(quint8 effectId)
{
    enqueue(makePacket(0x00, 0x03, effectId, 0x03, 0x00, 0x00, 0x00));
}

void BledomDevice::setEffectSpeed(int percent)
{
    percent = qBound(0, percent, 100);
    enqueue(makePacket(0x00, 0x02, quint8(percent), 0x00, 0x00, 0x00, 0x00));
}

void BledomDevice::syncTime()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QTime t = now.time();
    // Day of week: 1 = Monday ... 7 = Sunday
    enqueue(makePacket(0x00, 0x83, quint8(t.hour()), quint8(t.minute()), quint8(t.second()),
                       quint8(now.date().dayOfWeek()), 0x00));
}

void BledomDevice::sendRaw(const QByteArray &packet)
{
    if (packet.isEmpty())
        return;
    enqueue(packet);
}

void BledomDevice::enqueue(const QByteArray &packet)
{
    if (!isReady()) {
        emit errorOccurred(tr("Device is not connected"));
        return;
    }
    // One packet per command type (3rd byte): a newer one replaces the old.
    auto keyOf = [](const QByteArray &p) { return p.size() > 2 ? quint8(p.at(2)) : quint8(0xff); };
    const quint8 key = keyOf(packet);
    m_pending.removeIf([&](const QByteArray &p) { return keyOf(p) == key; });
    m_pending.append(packet);
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void BledomDevice::flushQueue()
{
    if (!isReady() || !m_service || !m_writeChar.isValid()) {
        m_pending.clear();
        return;
    }
    const auto packets = m_pending;
    m_pending.clear();
    for (const QByteArray &p : packets) {
        m_service->writeCharacteristic(m_writeChar, p, m_writeMode);
        emit packetSent(p);
    }
}
