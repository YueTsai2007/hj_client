#include "window.h"
#include "map.h"
#include "topics.h"
#include <QTimer>
#include "window_helpers.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QListWidget>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QStyle>
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

void updateRobotCard(QWidget *row, const QString &color, const QString &status, const QString &value, int percentage);

QFrame *robotCard(QListWidget *list, int id, const QString &color, const QString &status, const QString &value, int percentage = -1) {
    auto *row = new QFrame;
    row->setObjectName("robotCard");
    row->setStyleSheet(QStringLiteral(
        "QFrame#robotCard { background:rgba(29,38,50,145); border:1px solid #303d4e; border-radius:4px; }"));
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(7, 3, 7, 3);
    layout->setSpacing(3);
    auto *top = new QHBoxLayout;
    top->setSpacing(4);
    top->addWidget(label(QStringLiteral("%1 %2").arg(id).arg(robotName(id)), "robotName"));
    top->addStretch();
    auto *health = label(value, "robotValue");
    top->addWidget(health);
    layout->addLayout(top);
    auto *bar = new QProgressBar;
    bar->setRange(0, 1000);
    bar->setTextVisible(false);
    bar->setFixedHeight(5);
    layout->addWidget(bar);
    updateRobotCard(row, color, status, value, percentage);
    auto *item = new QListWidgetItem(list);
    item->setSizeHint(QSize(120, 38));
    list->setItemWidget(item, row);
    return row;
}
void updateRobotCard(QWidget *row, const QString &color, const QString &status, const QString &value, int percentage) {
    auto *health = row->findChild<QLabel *>("robotValue");
    health->setText(value == QStringLiteral("-- HP") ? status : value);
    row->setToolTip(status);
    health->setAccessibleDescription(status);
    auto *bar = row->findChild<QProgressBar *>();
    const QString style = QStringLiteral("QProgressBar {background:#263746;border:0;border-radius:3px;} QProgressBar::chunk {background:%1;}").arg(color);
    if (bar->styleSheet() != style) bar->setStyleSheet(style);
    bar->setValue(qMax(0, percentage));
    bar->setVisible(percentage >= 0);
}
} // namespace

void Window::setBaseHealth(QLabel *label, QProgressBar *bar, const QString &health) {
    bool currentOk = false;
    const auto current = health.toUInt(&currentOk);
    const int configuredMaximum = qEnvironmentVariableIntValue("HJ_BASE_MAX_HP");
    const int maximum = configuredMaximum > 0 ? configuredMaximum
        : demoMode_ ? 5000 : 0;
    label->setText(health);
    const bool valid = currentOk && maximum > 0 && current <= static_cast<uint>(maximum);
    bar->setFormat(health);
    if (valid) bar->setValue(static_cast<int>(quint64(current) * 1000 / maximum));
    else bar->reset();
    bar->update();
}

qint64 Window::dataAgeMs(const char *topic) {
    std::lock_guard<std::mutex> lock(recordsMutex_);
    const auto it = records_.find(topic);
    if (it == records_.end() || it->second.receivedTick < connectedAtTick_) return -1;
    return monotonicMs() - it->second.receivedTick;
}

void Window::markStale() {
    map_->expireRobotPositions();
    if (mapHud_) mapHud_->expireRobotPositions();
    bool maxExpired = false;
    for (auto &timestamp : maxHealthAt_) {
        if (timestamp >= 0 && maxClock_.elapsed() - timestamp > 3000) { timestamp = -1; maxExpired = true; }
    }
    if (maxExpired && !health_.empty()) showHealth(health_);

    // 过期不再清空：保留最后一次的值，只把颜色变灰，具体多久没更新由告警条给出。
    const auto apply = [this](const char *topic, qint64 thresholdMs, std::initializer_list<QLabel *> labels) {
        const qint64 age = dataAgeMs(topic);
        const bool stale = age >= 0 && age > thresholdMs;
        for (QLabel *label : labels) {
            if (!label || label->property("stale").toBool() == stale) continue;
            label->setProperty("stale", stale);
            label->setStyleSheet(stale ? QStringLiteral("color:#6d7885;") : QString());
        }
    };
    apply("GameStatus", topics::timeoutMs("GameStatus"), {scoreRed_, scoreBlue_, clock_, round_});
    apply("GlobalUnitStatus", topics::timeoutMs("GlobalUnitStatus"), {damage_});
    apply("GlobalLogisticsStatus", topics::timeoutMs("GlobalLogisticsStatus"), {economy_});
    apply("RobotPosition", topics::timeoutMs("RobotPosition"), {position_});
    apply("RadarInfoToClient", topics::timeoutMs("RadarInfoToClient"), {radar_});
    apply("RobotDynamicStatus", topics::timeoutMs("RobotDynamicStatus"), {localStatus_[0], localStatus_[1], localStatus_[2]});
    apply("RobotRespawnStatus", topics::timeoutMs("RobotRespawnStatus"), {localStatus_[3]});
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
        const bool valid = ownSide_ >= 0 && maxHealthAt_[static_cast<size_t>(i)] >= 0 && maximum > 0 && current <= maximum;
        const int percentage = valid ? static_cast<int>((static_cast<uint64_t>(current) * 1000) / maximum) : -1;
        const QString status = valid ? (current == 0 ? QStringLiteral("血量为零") : percentage <= 300 ? QStringLiteral("低血量") : QStringLiteral("血量正常"))
            : current == 0 ? QStringLiteral("血量为零") : maximum > 0 && maxHealthAt_[static_cast<size_t>(i)] >= 0 ? QStringLiteral("上限异常") : QStringLiteral("上限未知");
        const QString value = valid ? QStringLiteral("%1 / %2").arg(current).arg(maximum)
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
        const auto report = [this, &issues](const char *topic, qint64 thresholdMs, const char *name) {
            const qint64 age = dataAgeMs(topic);
            if (age < 0) issues << QString::fromUtf8(name) + QStringLiteral("未就绪");
            else if (age > thresholdMs) issues << QString::fromUtf8(name) + QStringLiteral("已 %1 秒未更新").arg(age / 1000);
        };
        report("GameStatus", topics::timeoutMs("GameStatus"), "比赛状态");
        report("GlobalUnitStatus", topics::timeoutMs("GlobalUnitStatus"), "机器人血量");
        report("GlobalLogisticsStatus", topics::timeoutMs("GlobalLogisticsStatus"), "经济");
        report("RobotDynamicStatus", topics::timeoutMs("RobotDynamicStatus"), "本机状态");
        report("RobotModuleStatus", topics::timeoutMs("RobotModuleStatus"), "模块状态");
        report("RadarInfoToClient", topics::timeoutMs("RadarInfoToClient"), "雷达");
        if (!health_.empty() && health_.size() != 8) issues << QStringLiteral("机器人血量数据长度异常");
    }
    if (video_ && !video_->running()) issues << QStringLiteral("图传接收器未启动");
    else if (!frameAge_.isValid()) issues.prepend(gotVideo_ ? QStringLiteral("图传已中断") : QStringLiteral("等待图传首帧"));
    if (localStatus_[0]->text() == QLatin1String("0"))
        issues << QStringLiteral("本机血量为零");
    // 顶部条已被“机制提示条”接管（在 refreshSummary 里维护），这里不再写文本。
    Q_UNUSED(issues);
}

void Window::refreshSummary() {
    updateCombatHud();
    if (exchangePanel_ && exchangePanel_->isVisible()) updateExchangePanel();
    if (topBar_) {
        const auto field = [this](const char *topic, const char *key, const QString &fallback = QStringLiteral("--")) {
            const auto value = messageFields(topic).value(QLatin1String(key));
            return value.isDouble() ? QString::number(value.toInt()) : fallback;
        };
        const bool blue = ownSide_ == 1;
        const auto postAlly = field("GlobalUnitStatus", blue ? "outpostBHealth" : "outpostRHealth");
        const auto postEnemy = field("GlobalUnitStatus", blue ? "outpostRHealth" : "outpostBHealth");
        const auto economy = field("GlobalLogisticsStatus", "remainingEconomy");
        const auto fortAlly = field("GlobalUnitStatus", blue ? "enemyBaseStatus" : "baseStatus");
        const auto fortEnemy = field("GlobalUnitStatus", blue ? "baseStatus" : "enemyBaseStatus");
        const auto respawnGold = field("RobotRespawnStatus", "goldCostForRespawn");
        const auto canPay = messageFields("RobotRespawnStatus").value(QStringLiteral("canPayForRespawn")).toBool();
        topBar_->setText(QStringLiteral(
            "<span style='color:#8da0b3'>前哨</span> <span style='color:#ff5a67'>%1</span> : <span style='color:#3ea8ff'>%2</span>"
            "　｜　<span style='color:#8da0b3'>经济</span> <span style='color:#ffc971'>%3</span>"
            "　｜　<span style='color:#8da0b3'>堡垒</span> %4 : %5"
            "　｜　<span style='color:#8da0b3'>买活</span> <span style='color:#ffc971'>%6</span> 金币 %7")
            .arg(postAlly, postEnemy, economy, fortAlly, fortEnemy, respawnGold,
                 canPay ? QStringLiteral("<span style='color:#7fe0a8'>可买</span>") : QStringLiteral("<span style='color:#6d7885'>不可买</span>")));
    }
    if (analysis_) {
        const auto number = [this](const char *topic, const char *key) -> qint64 {
            const auto value = messageFields(topic).value(QLatin1String(key));
            return value.isDouble() ? static_cast<qint64>(value.toDouble()) : 0;
        };
        const qint64 cpAlly = number("GlobalUnitStatus", "totalCPAlly");
        const qint64 cpEnemy = number("GlobalUnitStatus", "totalCPEnemy");
        const qint64 dmgAlly = number("GlobalUnitStatus", "totalDamageAlly");
        const qint64 dmgEnemy = number("GlobalUnitStatus", "totalDamageEnemy");
        qint64 hpAlly = 0, hpEnemy = 0;
        for (size_t i = 0; i < health_.size(); ++i) (i < 4 ? hpAlly : hpEnemy) += health_[i];
        const qint64 gold = number("GlobalLogisticsStatus", "remainingEconomy");
        const auto diff = [](qint64 a, qint64 b) {
            const qint64 d = a - b;
            const QString color = d >= 0 ? QStringLiteral("#7fe0a8") : QStringLiteral("#ff8a93");
            return QStringLiteral("<span style='color:%1'>%2%3</span>").arg(color,
                d >= 0 ? QStringLiteral("+") : QString(), QString::number(d));
        };
        // 参考 ClearWei：按“战局 / 资源”分组，用色条区分维度，数值加粗放大
        const auto row = [](const QString &accent, const QString &name, qint64 value, const QString &diffHtml) {
            return QStringLiteral(
                "<tr>"
                "<td style='color:%1;font-size:16px'>▍</td>"
                "<td style='color:#c7d2de;font-size:13px'>%2</td>"
                "<td align='right' style='color:#eaf1f7;font-size:19px;font-weight:700'>%3</td>"
                "<td align='right' style='font-size:14px;font-weight:700'>%4</td>"
                "</tr>").arg(accent, name, QString::number(value), diffHtml);
        };
        QString html = QStringLiteral(
            "<table width='100%' cellspacing='3'>"
            "<tr><td colspan='4' style='color:#8da0b3;font-size:11px;letter-spacing:2px'>战 局</td></tr>");
        html += row(QStringLiteral("#e8a33d"), QStringLiteral("控制度"), cpAlly, diff(cpAlly, cpEnemy));
        html += row(QStringLiteral("#ff5a67"), QStringLiteral("总伤害"), dmgAlly, diff(dmgAlly, dmgEnemy));
        html += row(QStringLiteral("#7fe0a8"), QStringLiteral("总血量"), hpAlly, diff(hpAlly, hpEnemy));
        html += QStringLiteral("<tr><td colspan='4' style='color:#8da0b3;font-size:11px;letter-spacing:2px'>资 源</td></tr>");
        html += row(QStringLiteral("#3ea8ff"), QStringLiteral("剩余经济"), gold, QString());
        html += QStringLiteral("</table>");
        analysis_->setText(html);
    }
    if (modulePanel_) {
        static const char *keys[8] = {"powerManager", "rfid", "lightStrip", "smallShooter",
                                      "bigShooter", "uwb", "armor", "videoTransmission"};
        const auto modules = messageFields("RobotModuleStatus");
        for (size_t i = 0; i < moduleDots_.size(); ++i) {
            const auto value = modules.value(QLatin1String(keys[i]));
            QString color = QStringLiteral("#6d7885");
            if (value.isDouble()) color = value.toInt() == 1 ? QStringLiteral("#7fe0a8") : QStringLiteral("#ff5a67");
            if (moduleDots_[i]) moduleDots_[i]->setStyleSheet(QStringLiteral("color:%1;font-size:12px;").arg(color));
        }
    }
    // 顶部提示条：机制倒计时（GlobalSpecialMechanism，数组形式）
    if (alert_) {
        const auto mechanism = messageFields("GlobalSpecialMechanism");
        const auto ids = mechanism.value(QStringLiteral("mechanismId")).toArray();
        const auto times = mechanism.value(QStringLiteral("mechanismTimeSec")).toArray();
        const auto nameOf = [](int id) -> QString {
            switch (id) {
            case 1: return QStringLiteral("己方堡垒被激活");
            case 2: return QStringLiteral("对方堡垒被激活");
            case 3: return QStringLiteral("R 前哨阵地激活");
            case 4: return QStringLiteral("B 前哨阵地激活");
            case 5: return QStringLiteral("R 基地阵地激活");
            case 6: return QStringLiteral("B 基地阵地激活");
            default: return QString();
            }
        };
        QStringList parts;
        for (int i = 0; i < ids.size() && i < times.size(); ++i) {
            const int id = ids.at(i).toInt();
            const int signedSeconds = times.at(i).toInt();
            const QString name = nameOf(id);
            if (name.isEmpty()) continue;
            QString detail;
            if (id >= 3) {
                // 3–6：符号表示推进方向（<0 对方推进，>0 己方推进，0 无变化）
                if (signedSeconds == 0) detail = QStringLiteral("无变化");
                else detail = QStringLiteral("%1 %2s")
                                  .arg(signedSeconds > 0 ? QStringLiteral("己方推进") : QStringLiteral("对方推进"))
                                  .arg(qAbs(signedSeconds));
            } else {
                if (signedSeconds <= 0) continue;
                detail = QStringLiteral("%1s").arg(signedSeconds);
            }
            parts << QStringLiteral("%1 %2").arg(name, detail);
        }
        if (parts.isEmpty()) {
            alert_->clear();
            alert_->setVisible(false);
        } else {
            alert_->setText(QStringLiteral("● ") + parts.join(QStringLiteral("　·　")));
            alert_->setVisible(true);
        }
    }
    const auto setLocal = [this](int index, const QString &text) {
        if (localStatus_[index]->text() != text) localStatus_[index]->setText(text);
    };
    if (roleInfo_) {
        const auto valueOf = [this](const char *topic, const char *key) -> QJsonValue {
            return messageFields(topic).value(QLatin1String(key));
        };
        const auto numberText = [](const QJsonValue &v) {
            return v.isDouble() ? QString::number(v.toInt()) : QStringLiteral("等待");
        };
        QString text;
        switch (role_) {
        case Role::Hero:
            text = QStringLiteral("科技核心状态：%1\n阵地计时：%2 秒\n装配难度 1~4 级，按比赛时间解锁")
                .arg(numberText(valueOf("TechCoreMotionStateSync", "basicState")),
                     numberText(valueOf("GlobalSpecialMechanism", "mechanismTimeSec")));
            break;
        case Role::Sentry:
            text = QStringLiteral("哨兵姿态：%1\n最近控制结果：%2")
                .arg(numberText(valueOf("SentryStatusSync", "postureId")),
                     numberText(valueOf("SentryCtrlResult", "resultCode")));
            break;
        case Role::Dart:
            text = QStringLiteral("飞镖：目标选择 / 开闸 / 发射的上行尚未接入");
            break;
        case Role::Aerial:
            text = QStringLiteral("空中：飞行状态显示待接入");
            break;
        case Role::Engineer:
            text = QStringLiteral("工程：取矿 / 装配提示待接入");
            break;
        default:
            break;
        }
        if (!text.isEmpty() && roleInfo_->text() != text) roleInfo_->setText(text);
    }
    if (!connected_) {
        for (int i = 0; i < 4; ++i) setLocal(i, QStringLiteral("断开"));
        const QString text = QStringLiteral("连接断开：机器人和场地状态不可用");
        if (situation_->count() != 1 || situation_->item(0)->text() != text) { situation_->clear(); situation_->addItem(text); }
        return;
    }
    struct SummaryField { const char *topic; const char *field; const char *title; bool triggered; };
    static const SummaryField fields[] = {
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
    const auto renderLocal = [&](const char *topic, int first, int count) {
        const auto it = snapshot.find(topic);
        const auto unavailable = [&](const QString &text) {
            for (int i = first; i < first + count; ++i) setLocal(i, text);
        };
        if (it == snapshot.end() || it->second.receivedTick < connectedAtTick_) {
            unavailable(QStringLiteral("等待数据"));
            return;
        }
        if (!it->second.error.empty()) {
            unavailable(QStringLiteral("数据异常"));
            return;
        }
        const auto &fields = it->second.fields;
        const std::string topicName(topic);
        if (topicName == "RobotDynamicStatus") {
            const auto number = [&fields](const char *key) {
                const auto value = fields.value(QLatin1String(key));
                return value.isDouble() ? QString::number(value.toDouble(), 'g', 12) : QStringLiteral("--");
            };
            setLocal(0, number("currentHealth"));
            setLocal(1, number("remainingAmmo"));
            setLocal(2, number("currentHeat"));
            return;
        }
        if (topicName == "RobotRespawnStatus") {
            if (!fields.value("isPendingRespawn").toBool()) {
                setLocal(3, QStringLiteral("无需复活"));
            } else if (fields.value("totalRespawnProgress").toInt() > 0) {
                setLocal(3, QStringLiteral("%1 / %2")
                    .arg(fields.value("currentRespawnProgress").toInt())
                    .arg(fields.value("totalRespawnProgress").toInt()));
            } else {
                setLocal(3, QStringLiteral("等待复活"));
            }
            return;
        }
        unavailable(QStringLiteral("数据异常"));
    };
    renderLocal("RobotDynamicStatus", 0, 3);
    renderLocal("RobotRespawnStatus", 3, 1);
    if (!details_ || !details_->isVisible()) return;
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
        const auto value = messageFields(it->second).value(QLatin1String(field.field));
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
