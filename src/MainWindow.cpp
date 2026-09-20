#include "MainWindow.h"
#include "Language.h"

#include <QActionGroup>
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QTime>
#include <QVBoxLayout>
#include <cstdio>

namespace {
const char *kDefaultMac = "BE:60:C6:00:05:1B";

QSlider *makeSlider(int max, int value)
{
    auto *s = new QSlider(Qt::Horizontal);
    s->setRange(0, max);
    s->setValue(value);
    return s;
}

QLabel *makeValueLabel()
{
    auto *l = new QLabel;
    l->setMinimumWidth(36);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return l;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dev(new BledomDevice(this))
{
    buildUi();
    loadSettings();

    connect(m_dev, &BledomDevice::deviceFound, this, &MainWindow::onDeviceFound);
    connect(m_dev, &BledomDevice::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_dev, &BledomDevice::logMessage, this, &MainWindow::onLog);
    connect(m_dev, &BledomDevice::errorOccurred, this, [this](const QString &m) {
        onLog(QStringLiteral("⚠ ") + m);
        statusBar()->showMessage(m, 5000);
    });
    connect(m_dev, &BledomDevice::packetSent, this, &MainWindow::onPacketSent);
    connect(m_dev, &BledomDevice::scanFinished, this, [this] {
        m_scanBtn->setEnabled(true);
        m_scanBtn->setText(tr("Scan"));
    });

    onStateChanged(BledomDevice::State::Disconnected);
    applyColor(m_color, false);
    retranslateUi();
}

void MainWindow::autoConnect(const QString &mac)
{
    m_macEdit->setText(mac.trimmed());
    onConnectToggle();
}

void MainWindow::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        retranslateUi();
    QMainWindow::changeEvent(e);
}

// ---------------------------------------------------------------- UI

void MainWindow::buildUi()
{
    buildMenu();

    auto *central = new QWidget;
    auto *root = new QVBoxLayout(central);

    root->addWidget(buildConnectionBox());
    root->addWidget(buildPowerBox());
    root->addWidget(buildColorBox());
    root->addWidget(buildEffectsBox());
    root->addWidget(buildExtrasBox());

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setMinimumHeight(120);
    root->addWidget(m_log, 1);

    setCentralWidget(central);
    statusBar();
    resize(520, 840);
}

void MainWindow::buildMenu()
{
    m_langMenu = menuBar()->addMenu(QString());
    m_langGroup = new QActionGroup(this);
    m_langGroup->setExclusive(true);

    const QString current = Language::saved();
    for (const QString &code : Language::available()) {
        QAction *a = m_langMenu->addAction(Language::displayName(code));
        a->setCheckable(true);
        a->setChecked(code == current);
        a->setData(code);
        m_langGroup->addAction(a);
    }
    connect(m_langGroup, &QActionGroup::triggered, this,
            [](QAction *a) { Language::apply(a->data().toString()); });
}

QWidget *MainWindow::buildConnectionBox()
{
    m_connBox = new QGroupBox;
    auto *g = new QGridLayout(m_connBox);

    m_deviceLbl = new QLabel;
    m_macLbl = new QLabel;
    m_statusTitleLbl = new QLabel;
    m_deviceCombo = new QComboBox;
    m_scanBtn = new QPushButton;
    m_macEdit = new QLineEdit;
    m_connectBtn = new QPushButton;
    m_statusLbl = new QLabel;

    g->addWidget(m_deviceLbl, 0, 0);
    g->addWidget(m_deviceCombo, 0, 1);
    g->addWidget(m_scanBtn, 0, 2);
    g->addWidget(m_macLbl, 1, 0);
    g->addWidget(m_macEdit, 1, 1);
    g->addWidget(m_connectBtn, 1, 2);
    g->addWidget(m_statusTitleLbl, 2, 0);
    g->addWidget(m_statusLbl, 2, 1, 1, 2);
    g->setColumnStretch(1, 1);

    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScan);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectToggle);
    connect(m_deviceCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0 && i < m_found.size())
            m_macEdit->setText(m_found[i].address().toString());
    });
    return m_connBox;
}

QWidget *MainWindow::buildPowerBox()
{
    m_powerBox = new QGroupBox;
    auto *h = new QHBoxLayout(m_powerBox);
    m_onBtn = new QPushButton;
    m_offBtn = new QPushButton;
    m_onBtn->setMinimumHeight(40);
    m_offBtn->setMinimumHeight(40);
    h->addWidget(m_onBtn);
    h->addWidget(m_offBtn);
    connect(m_onBtn, &QPushButton::clicked, this, [this] { m_dev->setPower(true); });
    connect(m_offBtn, &QPushButton::clicked, this, [this] { m_dev->setPower(false); });
    return m_powerBox;
}

QWidget *MainWindow::buildColorBox()
{
    m_colorBox = new QGroupBox;
    auto *v = new QVBoxLayout(m_colorBox);

    // Preview / picker button
    m_colorBtn = new QPushButton;
    m_colorBtn->setMinimumHeight(48);
    v->addWidget(m_colorBtn);
    connect(m_colorBtn, &QPushButton::clicked, this, &MainWindow::onPickColor);

    // Presets
    auto *presets = new QHBoxLayout;
    const QList<QColor> presetColors = {
        Qt::red, QColor(255, 128, 0), Qt::yellow, Qt::green, Qt::cyan,
        Qt::blue, QColor(160, 0, 255), Qt::magenta, QColor(255, 180, 120), Qt::white,
    };
    for (const QColor &c : presetColors) {
        auto *b = new QPushButton;
        b->setFixedSize(32, 24);
        b->setToolTip(c.name());
        b->setStyleSheet(QStringLiteral("background:%1; border:1px solid #555; border-radius:4px;")
                             .arg(c.name()));
        connect(b, &QPushButton::clicked, this, [this, c] { applyColor(c, true); });
        presets->addWidget(b);
    }
    presets->addStretch();
    v->addLayout(presets);

    // RGB sliders
    auto *g = new QGridLayout;
    auto addRow = [&](int row, const QString &name, QSlider *&slider, QLabel *&lbl) {
        slider = makeSlider(255, 255);
        lbl = makeValueLabel();
        g->addWidget(new QLabel(name), row, 0);
        g->addWidget(slider, row, 1);
        g->addWidget(lbl, row, 2);
        connect(slider, &QSlider::valueChanged, this, &MainWindow::onSliderColorChanged);
    };
    addRow(0, QStringLiteral("R"), m_rSlider, m_rLbl);
    addRow(1, QStringLiteral("G"), m_gSlider, m_gLbl);
    addRow(2, QStringLiteral("B"), m_bSlider, m_bLbl);

    m_brightTitleLbl = new QLabel;
    m_brightSlider = makeSlider(100, 100);
    m_brightLbl = makeValueLabel();
    g->addWidget(m_brightTitleLbl, 3, 0);
    g->addWidget(m_brightSlider, 3, 1);
    g->addWidget(m_brightLbl, 3, 2);
    connect(m_brightSlider, &QSlider::valueChanged, this, [this](int v) {
        m_brightLbl->setText(QStringLiteral("%1%").arg(v));
        m_dev->setBrightness(v);
    });
    g->setColumnStretch(1, 1);
    v->addLayout(g);
    return m_colorBox;
}

QWidget *MainWindow::buildEffectsBox()
{
    m_effectsBox = new QGroupBox;
    auto *g = new QGridLayout(m_effectsBox);

    m_effectTitleLbl = new QLabel;
    m_speedTitleLbl = new QLabel;
    m_effectCombo = new QComboBox;
    m_applyEffectBtn = new QPushButton;
    m_speedSlider = makeSlider(100, 50);
    m_speedLbl = makeValueLabel();
    m_speedLbl->setText(QStringLiteral("50%"));

    g->addWidget(m_effectTitleLbl, 0, 0);
    g->addWidget(m_effectCombo, 0, 1);
    g->addWidget(m_applyEffectBtn, 0, 2);
    g->addWidget(m_speedTitleLbl, 1, 0);
    g->addWidget(m_speedSlider, 1, 1);
    g->addWidget(m_speedLbl, 1, 2);
    g->setColumnStretch(1, 1);

    connect(m_applyEffectBtn, &QPushButton::clicked, this, [this] {
        m_dev->setEffect(quint8(m_effectCombo->currentData().toUInt()));
    });
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int v) {
        m_speedLbl->setText(QStringLiteral("%1%").arg(v));
        m_dev->setEffectSpeed(v);
    });
    return m_effectsBox;
}

QWidget *MainWindow::buildExtrasBox()
{
    m_extrasBox = new QGroupBox;
    auto *g = new QGridLayout(m_extrasBox);

    m_timeBtn = new QPushButton;
    m_rawLbl = new QLabel;
    m_rawEdit = new QLineEdit;
    m_sendRawBtn = new QPushButton;

    g->addWidget(m_timeBtn, 0, 0, 1, 3);
    g->addWidget(m_rawLbl, 1, 0);
    g->addWidget(m_rawEdit, 1, 1);
    g->addWidget(m_sendRawBtn, 1, 2);
    g->setColumnStretch(1, 1);

    connect(m_timeBtn, &QPushButton::clicked, m_dev, &BledomDevice::syncTime);
    connect(m_sendRawBtn, &QPushButton::clicked, this, &MainWindow::onSendRaw);
    connect(m_rawEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendRaw);
    return m_extrasBox;
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("ELK-BLEDOM LED Strip"));
    m_langMenu->setTitle(tr("&Language"));

    m_connBox->setTitle(tr("Connection"));
    m_deviceLbl->setText(tr("Device:"));
    m_macLbl->setText(tr("MAC:"));
    m_statusTitleLbl->setText(tr("Status:"));
    m_deviceCombo->setPlaceholderText(tr("- discovered devices -"));
    m_macEdit->setPlaceholderText(tr("MAC address, e.g. BE:60:C6:00:05:1B"));
    m_scanBtn->setText(m_scanBtn->isEnabled() ? tr("Scan") : tr("Scanning..."));
    updateStatusText();

    m_powerBox->setTitle(tr("Power"));
    m_onBtn->setText(tr("Turn on"));
    m_offBtn->setText(tr("Turn off"));

    m_colorBox->setTitle(tr("Color and brightness"));
    m_colorBtn->setText(tr("Pick color...   %1").arg(m_color.name().toUpper()));
    m_brightTitleLbl->setText(tr("Brightness"));

    m_effectsBox->setTitle(tr("Effects"));
    m_effectTitleLbl->setText(tr("Effect"));
    m_speedTitleLbl->setText(tr("Speed"));
    m_applyEffectBtn->setText(tr("Start"));
    {
        // Rebuild the effect list with translated names, keeping the selection.
        const int cur = qMax(0, m_effectCombo->currentIndex());
        m_effectCombo->blockSignals(true);
        m_effectCombo->clear();
        for (const auto &e : BledomDevice::effects())
            m_effectCombo->addItem(e.name, e.id);
        m_effectCombo->setCurrentIndex(cur);
        m_effectCombo->blockSignals(false);
    }

    m_extrasBox->setTitle(tr("Extras"));
    m_timeBtn->setText(tr("Sync time"));
    m_rawLbl->setText(tr("Raw:"));
    m_rawEdit->setPlaceholderText(tr("hex packet: 7e 00 05 03 ff 00 00 00 ef"));
    m_sendRawBtn->setText(tr("Send"));

    m_log->setPlaceholderText(tr("Log"));
}

void MainWindow::updateStatusText()
{
    using S = BledomDevice::State;
    QString text;
    switch (m_dev->state()) {
    case S::Disconnected: text = tr("not connected"); break;
    case S::Connecting:   text = tr("connecting..."); break;
    case S::Discovering:  text = tr("discovering services..."); break;
    case S::Ready:        text = tr("connected to %1").arg(m_dev->deviceAddress()); break;
    }
    m_statusLbl->setText(text);
    m_connectBtn->setText(m_dev->state() == S::Disconnected ? tr("Connect") : tr("Disconnect"));
}

// ---------------------------------------------------------------- slots

void MainWindow::onScan()
{
    m_found.clear();
    m_deviceCombo->clear();
    m_scanBtn->setEnabled(false);
    m_scanBtn->setText(tr("Scanning..."));
    m_dev->startScan();
}

void MainWindow::onDeviceFound(const QBluetoothDeviceInfo &info)
{
    for (const auto &f : m_found)
        if (f.address() == info.address())
            return;
    m_found.append(info);
    const QString name = info.name().isEmpty() ? tr("(no name)") : info.name();
    m_deviceCombo->addItem(QStringLiteral("%1  [%2]").arg(name, info.address().toString()));
    // Auto-select the strip if it shows up.
    if (name.contains(QStringLiteral("BLEDOM"), Qt::CaseInsensitive))
        m_deviceCombo->setCurrentIndex(m_found.size() - 1);
}

void MainWindow::onConnectToggle()
{
    if (m_dev->state() != BledomDevice::State::Disconnected) {
        m_dev->disconnectFromDevice();
        return;
    }
    const int idx = m_deviceCombo->currentIndex();
    const QString mac = m_macEdit->text().trimmed();
    if (idx >= 0 && idx < m_found.size() && m_found[idx].address().toString() == mac)
        m_dev->connectToDevice(m_found[idx]);
    else if (!mac.isEmpty())
        m_dev->connectToAddress(mac);
    else
        onLog(QStringLiteral("⚠ ") + tr("Enter a MAC address or pick a device"));
}

void MainWindow::onStateChanged(BledomDevice::State s)
{
    using S = BledomDevice::State;
    const bool ready = (s == S::Ready);
    const bool idle = (s == S::Disconnected);

    m_powerBox->setEnabled(ready);
    m_colorBox->setEnabled(ready);
    m_effectsBox->setEnabled(ready);
    m_extrasBox->setEnabled(ready);
    m_macEdit->setEnabled(idle);
    m_deviceCombo->setEnabled(idle);
    m_scanBtn->setEnabled(idle);

    updateStatusText();
    if (ready)
        saveSettings();
}

void MainWindow::onLog(const QString &msg)
{
    // Mirror to stderr so the CLI mode is usable from scripts.
    fprintf(stderr, "%s\n", qPrintable(msg));
    fflush(stderr);
    m_log->appendPlainText(QStringLiteral("[%1] %2")
                               .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), msg));
}

void MainWindow::onPacketSent(const QByteArray &p)
{
    onLog(QStringLiteral("→ %1").arg(QString::fromLatin1(p.toHex(' '))));
}

void MainWindow::applyColor(const QColor &c, bool send)
{
    m_color = c;
    m_updatingSliders = true;
    m_rSlider->setValue(c.red());
    m_gSlider->setValue(c.green());
    m_bSlider->setValue(c.blue());
    m_updatingSliders = false;
    m_rLbl->setText(QString::number(c.red()));
    m_gLbl->setText(QString::number(c.green()));
    m_bLbl->setText(QString::number(c.blue()));

    const QString fg = (c.lightness() > 140) ? QStringLiteral("#000") : QStringLiteral("#fff");
    m_colorBtn->setStyleSheet(
        QStringLiteral("background:%1; color:%2; border:1px solid #555; border-radius:6px; font-weight:bold;")
            .arg(c.name(), fg));
    m_colorBtn->setText(tr("Pick color...   %1").arg(c.name().toUpper()));

    if (send)
        m_dev->setColor(c);
}

void MainWindow::onSliderColorChanged()
{
    if (m_updatingSliders)
        return;
    applyColor(QColor(m_rSlider->value(), m_gSlider->value(), m_bSlider->value()), true);
}

void MainWindow::onPickColor()
{
    auto *dlg = new QColorDialog(m_color, this);
    dlg->setWindowTitle(tr("Strip color"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // Live preview on the strip while choosing.
    connect(dlg, &QColorDialog::currentColorChanged, this,
            [this](const QColor &c) { if (c.isValid()) applyColor(c, true); });
    connect(dlg, &QColorDialog::colorSelected, this,
            [this](const QColor &c) { if (c.isValid()) applyColor(c, true); });
    dlg->open();
}

void MainWindow::onSendRaw()
{
    QString s = m_rawEdit->text();
    s.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    if (s.isEmpty() || s.size() % 2 != 0) {
        onLog(QStringLiteral("⚠ ") + tr("Invalid hex"));
        return;
    }
    m_dev->sendRaw(QByteArray::fromHex(s.toLatin1()));
}

// ---------------------------------------------------------------- settings

void MainWindow::loadSettings()
{
    QSettings st;
    m_macEdit->setText(st.value(QStringLiteral("mac"), QString::fromLatin1(kDefaultMac)).toString());
    m_color = st.value(QStringLiteral("color"), QColor(Qt::white)).value<QColor>();
    m_brightSlider->blockSignals(true);
    m_brightSlider->setValue(st.value(QStringLiteral("brightness"), 100).toInt());
    m_brightSlider->blockSignals(false);
    m_brightLbl->setText(QStringLiteral("%1%").arg(m_brightSlider->value()));
}

void MainWindow::saveSettings()
{
    QSettings st;
    st.setValue(QStringLiteral("mac"), m_macEdit->text().trimmed());
    st.setValue(QStringLiteral("color"), m_color);
    st.setValue(QStringLiteral("brightness"), m_brightSlider->value());
}
