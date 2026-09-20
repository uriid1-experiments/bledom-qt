#pragma once

#include <QMainWindow>
#include <QBluetoothDeviceInfo>
#include <QColor>
#include <QList>

#include "BledomDevice.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QSlider;
class QPlainTextEdit;
class QGroupBox;
class QMenu;
class QActionGroup;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Fill the MAC field and start connecting (used by the --connect option).
    void autoConnect(const QString &mac);
    BledomDevice *device() const { return m_dev; }

protected:
    void changeEvent(QEvent *e) override;

private:
    void buildUi();
    void buildMenu();
    QWidget *buildConnectionBox();
    QWidget *buildPowerBox();
    QWidget *buildColorBox();
    QWidget *buildEffectsBox();
    QWidget *buildExtrasBox();

    // Re-applies all user-visible strings after a language switch.
    void retranslateUi();
    void updateStatusText();

    void onScan();
    void onConnectToggle();
    void onDeviceFound(const QBluetoothDeviceInfo &info);
    void onStateChanged(BledomDevice::State s);
    void onLog(const QString &msg);
    void onPacketSent(const QByteArray &p);

    void applyColor(const QColor &c, bool send);
    void onSliderColorChanged();
    void onPickColor();
    void onSendRaw();

    void loadSettings();
    void saveSettings();

    BledomDevice *m_dev = nullptr;
    QList<QBluetoothDeviceInfo> m_found;
    QColor m_color = QColor(255, 255, 255);
    bool m_updatingSliders = false;

    // menu
    QMenu *m_langMenu = nullptr;
    QActionGroup *m_langGroup = nullptr;

    // connection
    QGroupBox *m_connBox = nullptr;
    QLabel *m_deviceLbl = nullptr;
    QLabel *m_macLbl = nullptr;
    QLabel *m_statusTitleLbl = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QLineEdit *m_macEdit = nullptr;
    QPushButton *m_scanBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QLabel *m_statusLbl = nullptr;

    // power
    QGroupBox *m_powerBox = nullptr;
    QPushButton *m_onBtn = nullptr;
    QPushButton *m_offBtn = nullptr;

    // color & brightness
    QGroupBox *m_colorBox = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QSlider *m_rSlider = nullptr;
    QSlider *m_gSlider = nullptr;
    QSlider *m_bSlider = nullptr;
    QLabel *m_rLbl = nullptr;
    QLabel *m_gLbl = nullptr;
    QLabel *m_bLbl = nullptr;
    QLabel *m_brightTitleLbl = nullptr;
    QSlider *m_brightSlider = nullptr;
    QLabel *m_brightLbl = nullptr;

    // effects
    QGroupBox *m_effectsBox = nullptr;
    QLabel *m_effectTitleLbl = nullptr;
    QLabel *m_speedTitleLbl = nullptr;
    QComboBox *m_effectCombo = nullptr;
    QPushButton *m_applyEffectBtn = nullptr;
    QSlider *m_speedSlider = nullptr;
    QLabel *m_speedLbl = nullptr;

    // extras
    QGroupBox *m_extrasBox = nullptr;
    QPushButton *m_timeBtn = nullptr;
    QLabel *m_rawLbl = nullptr;
    QLineEdit *m_rawEdit = nullptr;
    QPushButton *m_sendRawBtn = nullptr;

    QPlainTextEdit *m_log = nullptr;
};
