#include <QApplication>
#include <QCommandLineParser>
#include <QColor>
#include <QTimer>

#include "Language.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("uriid1"));
    QCoreApplication::setApplicationName(QStringLiteral("bledom-qt"));
    QCoreApplication::setApplicationVersion(QStringLiteral(BLEDOM_QT_VERSION));

    Language::apply(Language::saved());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QCoreApplication::translate("main",
            "Control an ELK-BLEDOM LED strip over BLE.\n"
            "Example: bledom-qt -c BE:60:C6:00:05:1B --power on --color ff8800 --quit"));
    parser.addHelpOption();
    QCommandLineOption connectOpt({QStringLiteral("c"), QStringLiteral("connect")},
                                  QCoreApplication::translate("main", "Connect to the device on startup"),
                                  QStringLiteral("MAC"));
    QCommandLineOption powerOpt(QStringLiteral("power"),
                                QCoreApplication::translate("main", "Turn the strip on/off after connecting"),
                                QStringLiteral("on|off"));
    QCommandLineOption colorOpt(QStringLiteral("color"),
                                QCoreApplication::translate("main", "Set color after connecting"),
                                QStringLiteral("RRGGBB"));
    QCommandLineOption brightOpt(QStringLiteral("brightness"),
                                 QCoreApplication::translate("main", "Set brightness 0..100"),
                                 QStringLiteral("N"));
    QCommandLineOption quitOpt(QStringLiteral("quit"),
                               QCoreApplication::translate("main", "Exit after sending the commands (no GUI)"));
    parser.addOptions({connectOpt, powerOpt, colorOpt, brightOpt, quitOpt});
    parser.process(app);

    MainWindow w;
    const bool headless = parser.isSet(quitOpt);
    if (!headless)
        w.show();

    if (parser.isSet(connectOpt)) {
        BledomDevice *dev = w.device();
        QObject::connect(dev, &BledomDevice::stateChanged, &app,
                         [&](BledomDevice::State s) {
                             if (s != BledomDevice::State::Ready)
                                 return;
                             if (parser.isSet(powerOpt))
                                 dev->setPower(parser.value(powerOpt).compare(
                                                   QStringLiteral("off"), Qt::CaseInsensitive) != 0);
                             if (parser.isSet(colorOpt)) {
                                 QString c = parser.value(colorOpt);
                                 if (!c.startsWith('#'))
                                     c.prepend('#');
                                 dev->setColor(QColor(c));
                             }
                             if (parser.isSet(brightOpt))
                                 dev->setBrightness(parser.value(brightOpt).toInt());
                             if (headless)
                                 QTimer::singleShot(600, &app, [dev] { dev->disconnectFromDevice(); });
                         });
        if (headless) {
            QObject::connect(dev, &BledomDevice::stateChanged, &app, [&](BledomDevice::State s) {
                if (s == BledomDevice::State::Disconnected)
                    QTimer::singleShot(100, &app, &QCoreApplication::quit);
            });
            QTimer::singleShot(20000, &app, [] { QCoreApplication::exit(2); }); // safety timeout
        }
        w.autoConnect(parser.value(connectOpt));
    }
    return app.exec();
}
