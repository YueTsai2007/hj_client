#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

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
    // 默认窗口模式；F11 切换全屏、Esc 退出全屏（截图模式跳过，避免离屏平台改尺寸）
    if (qEnvironmentVariableIntValue("HJ_FULLSCREEN") == 1 && qEnvironmentVariableIsEmpty("HJ_SNAPSHOT"))
        window.showFullScreen();
    if (qEnvironmentVariableIntValue("HJ_MAXIMIZE") == 1) window.showMaximized();
    // 验收辅助：HJ_SNAPSHOT=<png 路径> 时，3 秒后截图并退出（不影响正常使用）。
    const QString snapshot = qEnvironmentVariable("HJ_SNAPSHOT");
    if (!snapshot.isEmpty()) {
        QTimer::singleShot(2500, &app, [&window, &app, snapshot] {
            if (window.isFullScreen() || window.isMaximized()) window.showNormal();
            const int w = qEnvironmentVariableIntValue("HJ_WIN_W");
            const int h = qEnvironmentVariableIntValue("HJ_WIN_H");
            if (w > 0 && h > 0) window.resize(w, h);
            QTimer::singleShot(500, &app, [&window, &app, snapshot] {
                qWarning("[snapshot] window size %dx%d", window.width(), window.height());
                window.grab().save(snapshot);
                QCoreApplication::quit();
            });
        });
    }
    return app.exec();
}
