#include "window.h"
#include "window_helpers.h"
#include <QDateTime>
#include <QDialog>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QSplitter>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString topicTitle(const QString &name) {
    static const QMap<QString, QString> titles = {
        {"GameStatus", QStringLiteral("比赛状态")}, {"GlobalUnitStatus", QStringLiteral("全局单位状态")},
        {"GlobalLogisticsStatus", QStringLiteral("经济与科技")}, {"GlobalSpecialMechanism", QStringLiteral("场地特殊机制")},
        {"Event", QStringLiteral("比赛事件")}, {"RobotInjuryStat", QStringLiteral("机器人受伤统计")},
        {"RobotRespawnStatus", QStringLiteral("复活状态")}, {"RobotStaticStatus", QStringLiteral("机器人静态属性")},
        {"RobotDynamicStatus", QStringLiteral("机器人动态状态")}, {"RobotModuleStatus", QStringLiteral("模块状态")},
        {"RobotPosition", QStringLiteral("机器人位置")}, {"Buff", QStringLiteral("机器人增益")},
        {"PenaltyInfo", QStringLiteral("判罚信息")}, {"RobotPathPlanInfo", QStringLiteral("路径规划")},
        {"MapClickInfo", QStringLiteral("地图点击信息")}, {"RadarInfoToClient", QStringLiteral("雷达目标位置")},
        {"CustomByteBlock", QStringLiteral("自定义二进制数据")}, {"TechCoreMotionStateSync", QStringLiteral("科技核心状态")},
        {"RobotPerformanceSelectionSync", QStringLiteral("性能选择状态")}, {"RuneStatusSync", QStringLiteral("能量机关状态")},
        {"SentryStatusSync", QStringLiteral("哨兵状态")}, {"DartSelectTargetStatusSync", QStringLiteral("飞镖目标状态")},
        {"SentryCtrlResult", QStringLiteral("哨兵控制结果")}
    };
    return titles.value(name, name);
}
} // namespace

void Window::showDetails() {
    if (details_) {
        details_->show(); details_->raise(); details_->activateWindow();
        return;
    }
    details_ = new QDialog(this);
    details_->setWindowTitle(QStringLiteral("下行消息详情"));
    details_->resize(950, 650);
    auto *layout = new QVBoxLayout(details_);
    auto *hint = label(QStringLiteral("显示最近一条原始消息中实际提供的字段；未列出的可选字段表示未提供。事件型消息仅在触发时更新；二进制字段使用 Base64。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *split = new QSplitter(details_);
    topics_ = new QListWidget(split);
    messageDetails_ = new QPlainTextEdit(split);
    messageDetails_->setReadOnly(true);
    messageDetails_->setLineWrapMode(QPlainTextEdit::NoWrap);
    split->addWidget(topics_); split->addWidget(messageDetails_);
    split->setStretchFactor(0, 1); split->setStretchFactor(1, 2);
    layout->addWidget(split);
    for (const auto &name : rm2027::downlinkNames()) {
        auto *item = new QListWidgetItem(QString::fromStdString(name), topics_);
        item->setData(Qt::UserRole, QString::fromStdString(name));
    }
    connect(topics_, &QListWidget::currentRowChanged, this, [this] { refreshDetails(); });
    auto *refreshTimer = new QTimer(details_);
    connect(refreshTimer, &QTimer::timeout, this, [this] { if (details_->isVisible()) refreshDetails(); });
    refreshTimer->start(500);
    topics_->setCurrentRow(0);
    details_->show();
}

void Window::refreshDetails() {
    if (!topics_ || !messageDetails_) return;
    const auto selected = topics_->currentItem();
    if (!selected) return;
    const auto name = selected->data(Qt::UserRole).toString().toStdString();
    MessageRecord record;
    bool found = false;
    std::map<std::string, MessageRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        snapshot = records_;
    }
    for (int i = 0; i < topics_->count(); ++i) {
        auto *item = topics_->item(i);
        const auto topic = item->data(Qt::UserRole).toString().toStdString();
        const auto it = snapshot.find(topic);
        const auto topicName = QString::fromStdString(topic);
        item->setText(topicTitle(topicName) + QStringLiteral(" / ") + topicName + (it == snapshot.end() ? QStringLiteral("  · 等待") : QStringLiteral("  · %1 条").arg(it->second.count)));
    }
    const auto it = snapshot.find(name);
    if (it != snapshot.end()) { record = it->second; found = true; }
    if (!found) {
        messageDetails_->setPlainText(QStringLiteral("等待消息：") + QString::fromStdString(name));
        return;
    }
    if (!record.error.empty()) {
        messageDetails_->setPlainText(QStringLiteral("原始字段解析失败：") + QString::fromStdString(record.error));
        return;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(record.json), &error);
    const QString payload = error.error == QJsonParseError::NoError
        ? QString::fromUtf8(doc.toJson(QJsonDocument::Indented))
        : QString::fromStdString(record.json);
    const auto time = QDateTime::fromMSecsSinceEpoch(record.receivedAt).toString("yyyy-MM-dd HH:mm:ss.zzz");
    const auto topicName = QString::fromStdString(name);
    const bool historical = !connected_ || record.receivedTick < connectedAtTick_;
    const QString text = QStringLiteral("消息：%1 / %2\n接收次数：%3\n最后接收：%4\n连接：%5\n\n%6")
        .arg(topicTitle(topicName), topicName).arg(record.count).arg(time, historical ? QStringLiteral("历史数据，当前连接尚未收到更新") : QStringLiteral("当前连接收到的数据"), payload);
    if (messageDetails_->toPlainText() != text) {
        const int scroll = messageDetails_->verticalScrollBar()->value();
        messageDetails_->setPlainText(text);
        messageDetails_->verticalScrollBar()->setValue(scroll);
    }
}
