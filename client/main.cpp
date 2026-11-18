#include "window.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("HJ_Client");
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("悍匠战场终端"));
    parser.addHelpOption();
    parser.addOption({"host", QStringLiteral("服务器地址"), "address", "127.0.0.1"});
    parser.addOption({"port", QStringLiteral("MQTT 端口"), "port", "3333"});
    parser.addOption({"id", QStringLiteral("机器人 ID"), "id", "3"});
    parser.addOption({"video-port", QStringLiteral("图传 UDP 端口"), "port", "3334"});
    parser.process(app);
    bool portOk = false, videoOk = false;
    const uint port = parser.value("port").toUInt(&portOk);
    const uint videoPort = parser.value("video-port").toUInt(&videoOk);
    if (!portOk || !videoOk || port == 0 || videoPort == 0 || port > 65535 || videoPort > 65535 || parser.value("id").isEmpty()) {
        qCritical("Invalid port or robot ID"); return 1;
    }
    Window window(parser.value("host"), static_cast<quint16>(port), parser.value("id"), static_cast<quint16>(videoPort));
    window.show();
    return app.exec();
}
