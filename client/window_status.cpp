#include "window.h"
#include "window_helpers.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QJsonArray>
#include <QJsonDocument>

namespace {
QString robotName(int id) {
    switch (id) {
    case 1: return QStringLiteral("重装");
    case 3: case 4: return QStringLiteral("步兵");
    case 7: return QStringLiteral("哨兵");
    default: return QStringLiteral("机器人");
    }
}

QFrame *robotCard(QListWidget *list, int id, const QString &color, const QString &status, const QString &value, int percentage = -1) {
    auto *row = new QFrame;
    row->setObjectName("robotCard");
    row->setStyleSheet(QStringLiteral("QFrame#robotCard { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 rgba(34,47,66,230), stop:1 rgba(16,25,37,230)); border: 1px solid rgba(232,163,61,60); border-left: 3px solid %1; border-radius: 9px; }").arg(color));
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 8, 12, 8);
    layout->setSpacing(5);
    auto *top = new QHBoxLayout;
    top->setSpacing(10);
    auto *badge = label(QString::number(id), "robotNum");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(30, 30);
    auto *name = label(robotName(id), "robotName");
    auto *state = label(status, "robotState");
    state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    top->addWidget(badge); top->addWidget(name); top->addStretch(); top->addWidget(state);
    layout->addLayout(top);
    layout->addWidget(label(value, "robotValue"));
    {
        auto *bar = new QProgressBar;
        bar->setRange(0, 1000); bar->setValue(qMax(0, percentage));
        bar->setTextVisible(false); bar->setFixedHeight(7);
        bar->setStyleSheet(QStringLiteral("QProgressBar {background:rgba(255,255,255,22);border:0;border-radius:3px;} QProgressBar::chunk {background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 %2,stop:1 %1);border-radius:3px;}")
                               .arg(color, QColor(color).darker(170).name()));
        layout->addWidget(bar);
    }
    auto *item = new QListWidgetItem(list);
    item->setSizeHint(row->sizeHint());
    list->setItemWidget(item, row);
    return row;
}
void updateRobotCard(QWidget *row, const QString &color, const QString &status, const QString &value, int percentage) {
    row->findChild<QLabel *>("robotState")->setText(status);
    row->findChild<QLabel *>("robotValue")->setText(value);
    auto *bar = row->findChild<QProgressBar *>();
    const QString style = QStringLiteral("QProgressBar {background:#263746;border:0;border-radius:3px;} QProgressBar::chunk {background:%1;}").arg(color);
    if (bar->styleSheet() != style) bar->setStyleSheet(style);
    bar->setValue(qMax(0, percentage));
}
} // namespace

void Window::markStale() {
    bool maxExpired = false;
    for (auto &timestamp : maxHealthAt_) {
        if (timestamp >= 0 && maxClock_.elapsed() - timestamp > 3000) { timestamp = -1; maxExpired = true; }
    }
    if (maxExpired && !health_.empty()) showHealth(health_);
    if (gameAge_.isValid() && gameAge_.elapsed() > 3000) {
        dataStatus_->setText(QStringLiteral("比赛数据已过期")); scoreRed_->setText("--"); scoreBlue_->setText("--"); clock_->setText("--:--"); round_->setText(QStringLiteral("比赛状态已过期")); gameAge_.invalidate();
    }
    if (unitAge_.isValid() && unitAge_.elapsed() > 3000) {
        base_->setText(QStringLiteral("基地血量　数据已过期")); damage_->setText(QStringLiteral("累计伤害　数据已过期")); health_.clear(); showHealthPlaceholder(QStringLiteral("数据已过期")); unitAge_.invalidate();
    }
    if (economyAge_.isValid() && economyAge_.elapsed() > 3000) { economy_->setText(QStringLiteral("经济数据已过期")); economyAge_.invalidate(); }
    if (positionAge_.isValid() && positionAge_.elapsed() > 3000) { position_->setText(QStringLiteral("机器人坐标：数据已过期")); positionAge_.invalidate(); }
    if (radarAge_.isValid() && radarAge_.elapsed() > 5000) { radar_->setText(QStringLiteral("雷达坐标：数据已过期")); radarAge_.invalidate(); }
}

void Window::showHealth(const std::vector<uint32_t> &health) {
    if (health.empty()) {
        showHealthPlaceholder(QStringLiteral("等待血量"));
        return;
    }
    if (health.size() != 8) {
        showHealthPlaceholder(QStringLiteral("数据异常"));
        return;
    }

    for (int i = 0; i < 8; ++i) {
        auto *list = i < 4 ? ally_ : enemy_;
        const auto current = health[static_cast<size_t>(i)];
        const auto maximum = maxHealth_[static_cast<size_t>(i)];
        const bool valid = maxHealthAt_[static_cast<size_t>(i)] >= 0 && maximum > 0 && current <= maximum;
        const int percentage = valid ? static_cast<int>((static_cast<uint64_t>(current) * 1000) / maximum) : -1;
        const QString status = valid ? (current == 0 ? QStringLiteral("血量为零") : percentage <= 300 ? QStringLiteral("低血量") : QStringLiteral("血量正常"))
            : maximum > 0 && maxHealthAt_[static_cast<size_t>(i)] >= 0 ? QStringLiteral("上限异常") : QStringLiteral("上限未知");
        const QString value = valid ? QStringLiteral("%1 / %2 HP  ·  %3%").arg(current).arg(maximum).arg(percentage / 10)
                                    : QStringLiteral("%1 HP").arg(current);
        const QString color = valid && percentage <= 300 ? QStringLiteral("#ffc46a") : teamColor(i < 4, ownSide_);
        updateRobotCard(list->itemWidget(list->item(i % 4)), color, status, value, percentage);
    }
}

void Window::showHealthPlaceholder(const QString &status) {
    if (ally_->count() == 4 && enemy_->count() == 4) {
        for (auto *list : {ally_, enemy_}) {
            for (int i = 0; i < 4; ++i)
                updateRobotCard(list->itemWidget(list->item(i)), teamColor(list == ally_, ownSide_), status, QStringLiteral("-- HP"), -1);
        }
        return;
    }
    ally_->clear(); enemy_->clear();
    for (int id : {1, 3, 4, 7}) {
        robotCard(ally_, id, teamColor(true, ownSide_), status, QStringLiteral("-- HP"));
        robotCard(enemy_, id, teamColor(false, ownSide_), status, QStringLiteral("-- HP"));
    }
}

void Window::updateAlert() {
    QStringList issues;
    if (!connected_) {
        issues << QStringLiteral("通信未连接");
    } else {
        if (!gameAge_.isValid()) issues << QStringLiteral("比赛状态未就绪或已过期");
        if (!unitAge_.isValid() || health_.empty()) issues << QStringLiteral("机器人血量未就绪或已过期");
        else if (health_.size() != 8) issues << QStringLiteral("机器人血量数据长度异常");
    }
    if (video_ && !video_->running()) issues << QStringLiteral("图传接收器未启动");
    else if (!frameAge_.isValid()) issues << QStringLiteral("图传尚无有效画面");
    const QString text = issues.isEmpty() ? QStringLiteral("●  通信与图传数据已接收") : QStringLiteral("●  ") + issues.join(QStringLiteral("   ·   "));
    if (alert_->text() != text) alert_->setText(text);
    if (alert_->property("ok").toBool() != issues.isEmpty()) {
        alert_->setProperty("ok", issues.isEmpty());
        alert_->style()->unpolish(alert_); alert_->style()->polish(alert_);
    }
}

void Window::refreshSummary() {
    if (!connected_) {
        const QString text = QStringLiteral("连接断开：机器人和场地状态不可用");
        if (situation_->count() != 1 || situation_->item(0)->text() != text) { situation_->clear(); situation_->addItem(text); }
        return;
    }
    struct SummaryField { const char *topic; const char *field; const char *title; bool triggered; };
    static const SummaryField fields[] = {
        {"RobotDynamicStatus", "currentHealth", "本机血量", false},
        {"RobotDynamicStatus", "currentHeat", "当前热量", false},
        {"RobotDynamicStatus", "remainingAmmo", "剩余弹量", false},
        {"RobotRespawnStatus", "currentRespawnProgress", "复活进度（原始值）", false},
        {"GlobalSpecialMechanism", "mechanismTimeSec", "阵地计时（秒，保留正负）", false},
        {"TechCoreMotionStateSync", "basicState", "科技核心状态（原始值）", false},
        {"RuneStatusSync", "runeStatus", "能量机关状态（原始值）", false},
        {"SentryStatusSync", "postureId", "哨兵姿态（原始值）", false},
        {"DartSelectTargetStatusSync", "targetId", "飞镖目标（原始值）", false},
        {"PenaltyInfo", "penaltyType", "最近判罚（原始值）", true},
        {"SentryCtrlResult", "resultCode", "最近哨兵控制结果（原始值）", true}
    };
    std::map<std::string, MessageRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        snapshot = records_;
    }
    QStringList lines;
    const auto now = monotonicMs();
    for (const auto &field : fields) {
        const auto it = snapshot.find(field.topic);
        if (it == snapshot.end()) continue;
        if (!it->second.error.empty()) {
            lines << QString::fromUtf8(field.title) + QStringLiteral("：原始字段解析失败");
            continue;
        }
        if (it->second.receivedTick < connectedAtTick_) {
            lines << QString::fromUtf8(field.title) + QStringLiteral("：等待当前连接更新");
            continue;
        }
        if (!field.triggered && now - it->second.receivedTick > 3000) {
            lines << QString::fromUtf8(field.title) + QStringLiteral("：数据已过期");
            continue;
        }
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(it->second.json));
        const auto value = doc.object().value(QLatin1String(field.field));
        QString text;
        if (value.isArray()) text = QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        else if (value.isString()) text = value.toString();
        else if (value.isDouble()) text = QString::number(value.toDouble());
        else if (value.isBool()) text = value.toBool() ? QStringLiteral("是") : QStringLiteral("否");
        else text = QStringLiteral("未提供");
        lines << QString::fromUtf8(field.title) + QStringLiteral("：") + text;
    }
    if (lines.isEmpty()) lines << QStringLiteral("机器人和场地状态：等待数据");
    QStringList current;
    for (int i = 0; i < situation_->count(); ++i) current << situation_->item(i)->text();
    if (current != lines) {
        const int scroll = situation_->verticalScrollBar()->value();
        situation_->clear(); situation_->addItems(lines);
        situation_->verticalScrollBar()->setValue(scroll);
    }
}
