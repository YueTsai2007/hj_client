#include "window.h"
#include "window_helpers.h"
#include "field.h"
#include "combat_view.h"
#include "brand.h"
#include "map.h"
#include "video_view.h"
#include <QStyle>
#include "rm2027/api/messages.h"
#include <QDateTime>
#include <QImage>
#include <QListWidget>
#include <QMetaObject>
#include <QPointer>
#include <QStackedWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QUuid>

namespace {
QString timeText(int seconds) {
    if (seconds < 0) return QString::number(seconds) + QStringLiteral(" 秒");
    return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
QString fieldValue(const QJsonObject &fields, const char *key) {
    const auto value = fields.value(QLatin1String(key));
    if (value.isUndefined() || value.isNull()) return QStringLiteral("--");
    if (value.isBool()) return value.toBool() ? QStringLiteral("是") : QStringLiteral("否");
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 12);
    return QStringLiteral("--");
}
QString stageText(const QJsonObject &fields) {
    if (!fields.contains("currentStage")) return QStringLiteral("阶段未提供");
    switch (fields.value("currentStage").toInt(-1)) {
    case 0: return QStringLiteral("未开始");
    case 1: return QStringLiteral("准备阶段");
    case 2: return QStringLiteral("十五秒自检");
    case 3: return QStringLiteral("五秒倒计时");
    case 4: return QStringLiteral("比赛中");
    case 5: return QStringLiteral("结算中");
    default: return QStringLiteral("未知阶段 %1").arg(fieldValue(fields, "currentStage"));
    }
}
} // namespace

void Window::start(const QString &host, quint16 port, const QString &id, quint16 videoPort) {
    bool idOk = false;
    const uint clientId = id.toUInt(&idOk);
    if (idOk && (clientId == 1 || clientId == 3 || clientId == 4 || clientId == 5 || clientId == 6 || clientId == 7)) ownSide_ = 0;
    if (idOk && (clientId == 101 || clientId == 103 || clientId == 104 || clientId == 105 || clientId == 106 || clientId == 107)) ownSide_ = 1;
    if (auto *allyPanel = findChild<GoldFrame *>("allyPanel")) allyPanel->setAccent(QColor(teamColor(true, ownSide_)));
    if (auto *enemyPanel = findChild<GoldFrame *>("enemyPanel")) enemyPanel->setAccent(QColor(teamColor(false, ownSide_)));
    showHealthPlaceholder(QStringLiteral("等待数据"));
    maxClock_.start();
    rm2027::Client::Options opt;
    opt.host = host.toStdString();
    opt.port = port;
    opt.clientId = id.toStdString();
    if (demoMode_) {
        const QString session = QUuid::createUuid().toString(QUuid::Id128);
        opt.clientId = (QStringLiteral("hj-demo-") + session.left(15)).toStdString();
    }
    opt.subscribeAllDownlink = true;
    opt.logMessages = false;
    client_ = std::make_unique<rm2027::Client>(opt);
    QPointer<Window> self(this);
    client_->onAny([self](std::unique_ptr<rm2027::Message> message) {
        if (self) self->recordMessage(std::move(message));
    });
    client_->onConnectionChanged([self](bool connected) {
        if (!self) return;
        const auto changedAt = monotonicMs();
        QMetaObject::invokeMethod(self, [self, connected, changedAt] {
            if (!self) return;
            self->connected_ = connected;
            if (connected) {
                self->connectedAtTick_ = changedAt;
                self->dataStatus_->setText(QStringLiteral("等待比赛首包"));
            }
            self->connection_->setText(connected ? QStringLiteral("● 已连接") : QStringLiteral("● 连接断开 / 重试中"));
            self->connection_->setProperty("up", connected);
            self->connection_->style()->unpolish(self->connection_); self->connection_->style()->polish(self->connection_);
            if (!connected) {
                self->maxHealthAt_.fill(-1);
                self->health_.clear();
                self->scoreRed_->setText("--"); self->scoreBlue_->setText("--"); self->clock_->setText("--:--");
                self->round_->setText(QStringLiteral("连接断开")); self->dataStatus_->setText(QStringLiteral("连接断开，等待新数据"));
                self->setBaseHealth(self->baseRed_, self->baseRedBar_, QStringLiteral("--"));
                self->setBaseHealth(self->baseBlue_, self->baseBlueBar_, QStringLiteral("--"));
                self->lastStage_.clear(); self->damage_->setText(QStringLiteral("累计伤害　连接断开"));
                self->economy_->setText(QStringLiteral("经济数据　连接断开"));
                self->showHealthPlaceholder(QStringLiteral("连接断开"));
                self->position_->setText(QStringLiteral("机器人坐标：连接断开")); self->radar_->setText(QStringLiteral("雷达坐标：连接断开"));
                self->gameAge_.invalidate(); self->unitAge_.invalidate(); self->economyAge_.invalidate();
                self->map_->clearRobotPositions();
                if (self->mapHud_) self->mapHud_->clearRobotPositions();
                self->positionAge_.invalidate(); self->radarAge_.invalidate();
                self->refreshSummary();
            }
            self->updateAlert();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GameStatus>([self](std::unique_ptr<rm2027::GameStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::GameStatus>(data));
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self) return;
            self->scoreRed_->setText(fieldValue(fields, "redScore")); self->scoreBlue_->setText(fieldValue(fields, "blueScore"));
            self->round_->setText(QStringLiteral("第 %1 / %2 局 · %3%4").arg(fieldValue(fields, "currentRound"), fieldValue(fields, "totalRounds"), stageText(fields), fields.contains("isPaused") && data.is_paused ? QStringLiteral(" · 暂停") : QString()));
            if (fields.contains("currentStage")) {
                const QString stage = self->round_->text();
                if (stage != self->lastStage_) {
                    self->lastStage_ = stage;
                    self->addEvent(stage);
                }
            }
            self->clock_->setText(fields.contains("stageCountdownSec") ? timeText(data.stage_countdown_sec) : QStringLiteral("--:--")); self->gameAge_.restart(); self->dataStatus_->setText(QStringLiteral("比赛状态已更新"));
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GlobalUnitStatus>([self](std::unique_ptr<rm2027::GlobalUnitStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::GlobalUnitStatus>(data));
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self) return;
            if (self->ownSide_ >= 0) {
                self->setBaseHealth(self->baseRed_, self->baseRedBar_, fieldValue(fields, self->ownSide_ == 0 ? "baseHealth" : "enemyBaseHealth"));
                self->setBaseHealth(self->baseBlue_, self->baseBlueBar_, fieldValue(fields, self->ownSide_ == 0 ? "enemyBaseHealth" : "baseHealth"));
            }
            self->damage_->setText(QStringLiteral("控制度 %3 / %4　·　累计伤害 %1 / %2").arg(fieldValue(fields, "totalDamageAlly"), fieldValue(fields, "totalDamageEnemy")).arg(fieldValue(fields, "totalCPAlly"), fieldValue(fields, "totalCPEnemy")));
            self->health_ = data.robot_health;
            self->showHealth(self->health_);
            self->unitAge_.restart();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GlobalLogisticsStatus>([self](std::unique_ptr<rm2027::GlobalLogisticsStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::GlobalLogisticsStatus>(data));
        QMetaObject::invokeMethod(self, [self, fields] { if (!self) return; self->economy_->setText(QStringLiteral("剩余经济 %1　/　科技等级 %2").arg(fieldValue(fields, "remainingEconomy"), fieldValue(fields, "techLevel"))); self->economyAge_.restart(); }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RobotStaticStatus>([self](std::unique_ptr<rm2027::RobotStaticStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::RobotStaticStatus>(data));
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self || self->ownSide_ < 0) return;
            if (!fields.contains("robotId")) return;
            const int ids[] = {1, 3, 4, 7};
            const int side = data.robot_id >= 100 ? 1 : 0;
            const int number = side ? static_cast<int>(data.robot_id) - 100 : static_cast<int>(data.robot_id);
            for (int i = 0; i < 4; ++i) {
                if (number != ids[i] || data.robot_id != static_cast<uint32_t>(number + side * 100)) continue;
                const int index = (side == self->ownSide_ ? 0 : 4) + i;
                self->maxHealth_[index] = data.max_health;
                self->maxHealthAt_[index] = fields.contains("maxHealth") && data.max_health > 0 ? self->maxClock_.elapsed() : -1;
                if (!self->health_.empty()) self->showHealth(self->health_);
                break;
            }
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::Event>([self](std::unique_ptr<rm2027::Event> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::Event>(data));
        QMetaObject::invokeMethod(self, [self, fields] {
            if (!self) return;
            self->addEvent(QStringLiteral("事件 %1  %2").arg(fieldValue(fields, "eventId"), fieldValue(fields, "param")));
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RobotPosition>([self](std::unique_ptr<rm2027::RobotPosition> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::RobotPosition>(data));
        QMetaObject::invokeMethod(self, [self, fields] {
            if (!self) return;
            self->position_->setText(QStringLiteral("机器人 %1：X %2　Y %3　Z %4　朝向 %5（原始值）")
                .arg(fieldValue(fields, "robotId"), fieldValue(fields, "x"), fieldValue(fields, "y"), fieldValue(fields, "z"), fieldValue(fields, "yaw")));
            self->positionAge_.restart();
            if (self->connected_ &&
                fields.value("robotId").isDouble() && fields.value("x").isDouble() && fields.value("y").isDouble()) {
                // 协议 0x0203：坐标为米，原点在红方补给站一角，X 向蓝方（场地 28m×15m）。
                // demo 服务器直接发 0~1 归一化值，这里按量级自动判断。
                qreal x = fields.value("x").toDouble();
                qreal y = fields.value("y").toDouble();
                if (x > 1.5 || y > 1.5) { x = field::normX(x); y = field::normY(y); }
                if (x >= 0 && x <= 1 && y >= 0 && y <= 1) {
                    const uint robotId = static_cast<uint>(fields.value("robotId").toDouble());
                    const QPointF pos(x, y);
                    const qreal yaw = fields.value("yaw").isDouble() ? fields.value("yaw").toDouble() : qQNaN();
                    self->map_->setRobotPosition(robotId, pos, yaw);
                    if (self->mapHud_) self->mapHud_->setRobotPosition(robotId, pos, yaw);
                }
            }
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RadarInfoToClient>([self](std::unique_ptr<rm2027::RadarInfoToClient> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::RadarInfoToClient>(data));
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self) return;
            if (data.radar_single_robot_info.size() != 8) {
                self->radar_->setText(QStringLiteral("雷达数据长度异常：%1（预期 8）").arg(data.radar_single_robot_info.size()));
                self->radarAge_.invalidate(); return;
            }
            QStringList rows;
            const int ids[] = {1, 3, 4, 7};
            const auto original = fields.value("radarSingleRobotInfo").toArray();
            if (original.size() != 8 && original.size() != 24) {
                self->radar_->setText(QStringLiteral("雷达原始字段不可用，请检查线格式"));
                self->radarAge_.invalidate(); return;
            }
            for (int i = 0; i < 8; ++i) {
                const auto &p = data.radar_single_robot_info[static_cast<size_t>(i)];
                const auto item = i < original.size() ? original.at(i) : QJsonValue();
                const auto nested = item.toObject();
                const QString x = item.isObject() ? fieldValue(nested, "targetPosX") : QString::number(p.target_pos_x);
                const QString y = item.isObject() ? fieldValue(nested, "targetPosY") : QString::number(p.target_pos_y);
                rows << QStringLiteral("%1%2: %3,%4").arg(i < 4 ? QStringLiteral("敌") : QStringLiteral("我"))
                    .arg(ids[i % 4]).arg(x, y);
            }
            self->radar_->setText(QStringLiteral("雷达坐标（厘米）：") + rows.join(QStringLiteral("  ")));
            self->radarAge_.restart();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::PenaltyInfo>([self](std::unique_ptr<rm2027::PenaltyInfo> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = toFields(*msg);
        self->recordMessage(std::make_unique<rm2027::PenaltyInfo>(data));
        QMetaObject::invokeMethod(self, [self, fields] {
            if (self) self->addEvent(QStringLiteral("判罚 %1 · %2 秒 · 累计 %3 次")
                .arg(fieldValue(fields, "penaltyType"), fieldValue(fields, "penaltyEffectSec"),
                     fieldValue(fields, "totalPenaltyNum")));
        }, Qt::QueuedConnection);
    });
    std::string error;
    if (!client_->start(&error)) connection_->setText(QStringLiteral("启动失败：") + QString::fromStdString(error));
    rm2027::VideoReceiverOptions videoOptions; videoOptions.port = videoPort;
    video_ = std::make_unique<rm2027::VideoReceiver>(videoOptions);
    if (!video_->start(&error)) {
        videoStatus_->setText(QStringLiteral("启动失败：") + QString::fromStdString(error));
        videoLink_->setText(QStringLiteral("图传 · 启动失败"));
        videoView_->setPlaceholder(QStringLiteral("图传启动失败"));
        if (combatVideo_) combatVideo_->setPlaceholder(QStringLiteral("图传启动失败"));
    }
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &Window::updateVideo);
    timer_->start(33);
    auto *summaryTimer = new QTimer(this);
    connect(summaryTimer, &QTimer::timeout, this, [this] { markStale(); refreshSummary(); updateAlert(); });
    summaryTimer->start(200);
    QTimer::singleShot(400, this, &Window::applyDefaultLayout);
}
void Window::addEvent(const QString &text) {
    const QString line = QDateTime::currentDateTime().toString("HH:mm:ss") + QStringLiteral("  ") + text;
    auto *item = new QListWidgetItem(line, events_);
    // 参考 ClearWei：判罚=红、兑换/经济=绿、阶段/比分=蓝、其它事件=金
    QColor color("#ffc971");
    if (text.contains(QStringLiteral("判罚")) || text.contains(QStringLiteral("扣血")) || text.contains(QStringLiteral("罚")))
        color = QColor("#ff5a67");
    else if (text.contains(QStringLiteral("兑换")) || text.contains(QStringLiteral("经济")) || text.contains(QStringLiteral("金币")))
        color = QColor("#7fe0a8");
    else if (text.contains(QStringLiteral("局")) || text.contains(QStringLiteral("阶段")) || text.contains(QStringLiteral("比分")))
        color = QColor("#6fb6ff");
    item->setForeground(color);
    events_->insertItem(0, item);
    while (events_->count() > 100) delete events_->takeItem(events_->count() - 1);
}

void Window::updateVideo() {
    if (!video_ || !video_->running()) return;
    rm2027::VideoFrame info;
    if (video_->getLatestFrame(&videoSequence_, &frame_, &info) && info.width > 0 && info.height > 0 &&
        info.stride >= static_cast<qint64>(info.width) * 3 &&
        frame_.size() >= static_cast<size_t>(info.stride) * info.height) {
        const QImage image(frame_.data(), info.width, info.height, info.stride, QImage::Format_BGR888);
        const bool combatVisible = mainStack_ && combat_ && mainStack_->currentWidget() == combat_;
        VideoView *target = combatVisible ? combatVideo_ : videoView_;
        if (target) target->setFrame(image.copy());
        frameAge_.restart();
        gotVideo_ = true;
        videoLink_->setText(QStringLiteral("图传 · 正常"));
        videoStatus_->setText(QStringLiteral("接收中 · %1 × %2").arg(info.width).arg(info.height));
    } else if (frameAge_.isValid() && frameAge_.elapsed() > 2000) {
        videoView_->setPlaceholder(QStringLiteral("图传已中断"));
        if (combatVideo_) combatVideo_->setPlaceholder(QStringLiteral("图传已中断"));
        videoLink_->setText(QStringLiteral("图传 · 中断"));
        videoStatus_->setText(QStringLiteral("等待恢复"));
        frameAge_.invalidate();
    } else if (!gotVideo_ && video_->stats().decodeErrors > 0) {
        videoStatus_->setText(QStringLiteral("图传解码错误，等待恢复"));
        videoLink_->setText(QStringLiteral("图传 · 解码异常"));
    }
}

QJsonObject Window::toFields(const rm2027::Message &message) {
    const auto json = rm2027::toJson(message);
    return QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
}

void Window::recordMessage(std::unique_ptr<rm2027::Message> message) {
    if (!message) return;
    const std::string topic = message->name();
    QJsonObject fields = toFields(*message);
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        auto &record = records_[topic];
        record.message = std::shared_ptr<const rm2027::Message>(std::move(message));
        record.fields = std::move(fields);
        record.error.clear();
        record.receivedAt = QDateTime::currentMSecsSinceEpoch();
        record.receivedTick = monotonicMs();
        ++record.count;
    }

    // 收到即更新：同一轮事件循环内多次消息合并成一次刷新
    if (!refreshPending_) {
        refreshPending_ = true;
        QMetaObject::invokeMethod(this, [this] {
            refreshPending_ = false;
            markStale();
            refreshSummary();
            updateAlert();
        }, Qt::QueuedConnection);
    }
}

QJsonObject Window::messageFields(const MessageRecord &record) {
    return record.fields;
}

QJsonObject Window::messageFields(const std::string &topic) {
    MessageRecord record;
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        const auto it = records_.find(topic);
        if (it == records_.end()) return {};
        record = it->second;
    }
    return messageFields(record);
}
