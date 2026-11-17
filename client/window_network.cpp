#include "window.h"
#include "window_helpers.h"
#include "brand.h"
#include "rm2027/api/messages.h"
#include "rm2027/api/detail/registry.h"
#include "rm2027/protocol/codec.h"
#include <google/protobuf/util/json_util.h>
#include <QDateTime>
#include <QImage>
#include <QListWidget>
#include <QMetaObject>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>

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
    opt.host = host.toStdString(); opt.port = port; opt.clientId = id.toStdString(); opt.subscribeAllDownlink = true; opt.logMessages = false;
    client_ = std::make_unique<rm2027::Client>(opt);
    QPointer<Window> self(this);
    client_->onRaw([self](const std::string &topic, const std::string &payload) {
        if (self) self->recordRaw(topic, payload);
    });
    client_->onConnectionChanged([self](bool connected) {
        if (!self) return;
        const auto changedAt = monotonicMs();
        QMetaObject::invokeMethod(self, [self, connected, changedAt] {
            if (!self) return;
            self->connected_ = connected;
            if (connected) self->connectedAtTick_ = changedAt;
            self->connection_->setText(connected ? QStringLiteral("● 已连接") : QStringLiteral("● 连接断开 / 重试中"));
            self->connection_->setProperty("up", connected);
            self->connection_->style()->unpolish(self->connection_); self->connection_->style()->polish(self->connection_);
            if (!connected) {
                self->maxHealthAt_.fill(-1);
                self->health_.clear();
                self->scoreRed_->setText("--"); self->scoreBlue_->setText("--"); self->clock_->setText("--:--");
                self->round_->setText(QStringLiteral("连接断开")); self->dataStatus_->setText(QStringLiteral("连接断开，等待新数据"));
                self->base_->setText(QStringLiteral("基地血量　连接断开")); self->damage_->setText(QStringLiteral("累计伤害　连接断开"));
                self->economy_->setText(QStringLiteral("经济数据　连接断开"));
                self->showHealthPlaceholder(QStringLiteral("连接断开"));
                self->position_->setText(QStringLiteral("机器人坐标：连接断开")); self->radar_->setText(QStringLiteral("雷达坐标：连接断开"));
                self->gameAge_.invalidate(); self->unitAge_.invalidate(); self->economyAge_.invalidate();
                self->positionAge_.invalidate(); self->radarAge_.invalidate();
                self->refreshSummary();
            }
            self->updateAlert();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GameStatus>([self](std::unique_ptr<rm2027::GameStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = self->messageFields(msg->name());
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self) return;
            self->scoreRed_->setText(fieldValue(fields, "redScore")); self->scoreBlue_->setText(fieldValue(fields, "blueScore"));
            self->round_->setText(QStringLiteral("第 %1 / %2 局 · %3%4").arg(fieldValue(fields, "currentRound"), fieldValue(fields, "totalRounds"), stageText(fields), fields.contains("isPaused") && data.is_paused ? QStringLiteral(" · 暂停") : QString()));
            self->clock_->setText(fields.contains("stageCountdownSec") ? timeText(data.stage_countdown_sec) : QStringLiteral("--:--")); self->gameAge_.restart(); self->dataStatus_->setText(QStringLiteral("比赛状态已更新"));
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GlobalUnitStatus>([self](std::unique_ptr<rm2027::GlobalUnitStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = self->messageFields(msg->name());
        QMetaObject::invokeMethod(self, [self, data, fields] {
            if (!self) return;
            self->base_->setText(QStringLiteral("红方基地 %1 HP　　　　　　 蓝方基地 %2 HP").arg(self->ownSide_ < 0 ? QStringLiteral("--") : fieldValue(fields, self->ownSide_ == 0 ? "baseHealth" : "enemyBaseHealth"), self->ownSide_ < 0 ? QStringLiteral("--") : fieldValue(fields, self->ownSide_ == 0 ? "enemyBaseHealth" : "baseHealth")));
            self->damage_->setText(QStringLiteral("控制度 %3 / %4　·　累计伤害 %1 / %2").arg(fieldValue(fields, "totalDamageAlly"), fieldValue(fields, "totalDamageEnemy")).arg(fieldValue(fields, "totalCPAlly"), fieldValue(fields, "totalCPEnemy")));
            self->health_ = data.robot_health;
            self->showHealth(self->health_);
            self->unitAge_.restart();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::GlobalLogisticsStatus>([self](std::unique_ptr<rm2027::GlobalLogisticsStatus> msg) {
        if (!self) return;
        const auto fields = self->messageFields(msg->name());
        QMetaObject::invokeMethod(self, [self, fields] { if (!self) return; self->economy_->setText(QStringLiteral("剩余经济 %1　/　科技等级 %2").arg(fieldValue(fields, "remainingEconomy"), fieldValue(fields, "techLevel"))); self->economyAge_.restart(); }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RobotStaticStatus>([self](std::unique_ptr<rm2027::RobotStaticStatus> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = self->messageFields(msg->name());
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
        const auto fields = self->messageFields(msg->name());
        QMetaObject::invokeMethod(self, [self, fields] {
            if (!self) return;
            self->events_->insertItem(0, QDateTime::currentDateTime().toString("HH:mm:ss") + QStringLiteral("  事件 %1  %2").arg(fieldValue(fields, "eventId"), fieldValue(fields, "param")));
            while (self->events_->count() > 100) delete self->events_->takeItem(self->events_->count() - 1);
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RobotPosition>([self](std::unique_ptr<rm2027::RobotPosition> msg) {
        if (!self) return;
        const auto fields = self->messageFields(msg->name());
        QMetaObject::invokeMethod(self, [self, fields] {
            if (!self) return;
            self->position_->setText(QStringLiteral("机器人 %1：X %2　Y %3　Z %4　朝向 %5（原始值）")
                .arg(fieldValue(fields, "robotId"), fieldValue(fields, "x"), fieldValue(fields, "y"), fieldValue(fields, "z"), fieldValue(fields, "yaw")));
            self->positionAge_.restart();
        }, Qt::QueuedConnection);
    });
    client_->on<rm2027::RadarInfoToClient>([self](std::unique_ptr<rm2027::RadarInfoToClient> msg) {
        if (!self) return;
        const auto data = *msg; const auto fields = self->messageFields(msg->name());
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
    client_->onAny([](std::unique_ptr<rm2027::Message>) {});
    std::string error;
    if (!client_->start(&error)) connection_->setText(QStringLiteral("启动失败：") + QString::fromStdString(error));
    rm2027::VideoReceiverOptions videoOptions; videoOptions.port = videoPort;
    video_ = std::make_unique<rm2027::VideoReceiver>(videoOptions);
    if (!video_->start(&error)) videoStatus_->setText(QStringLiteral("图传启动失败：") + QString::fromStdString(error));
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this] { updateVideo(); markStale(); });
    timer_->start(33);
    auto *summaryTimer = new QTimer(this);
    connect(summaryTimer, &QTimer::timeout, this, [this] { refreshSummary(); updateAlert(); });
    summaryTimer->start(500);
}
void Window::updateVideo() {
    if (!video_ || !video_->running()) return;
    rm2027::VideoFrame info;
    if (video_->getLatestFrame(&videoSequence_, &frame_, &info) && info.width > 0 && info.height > 0 && info.stride >= info.width * 3 && frame_.size() >= static_cast<size_t>(info.stride) * info.height) {
        const QImage image(frame_.data(), info.width, info.height, info.stride, QImage::Format_BGR888);
        videoView_->setPixmap(QPixmap::fromImage(image.copy()).scaled(videoView_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        frameAge_.restart();
        videoStatus_->setText(QStringLiteral("接收中 · %1 × %2").arg(info.width).arg(info.height));
    } else if (frameAge_.isValid() && frameAge_.elapsed() > 2000) {
        videoView_->setPixmap(QPixmap()); videoView_->setText(QStringLiteral("图传画面已中断")); videoStatus_->setText(QStringLiteral("等待新画面")); frameAge_.invalidate();
    } else if (!frameAge_.isValid() && video_->stats().decodeErrors > 0) {
        videoStatus_->setText(QStringLiteral("图传解码错误，等待恢复"));
    }
}

void Window::recordRaw(const std::string &topic, const std::string &payload) {
    const auto *desc = rm2027::findMessage(topic);
    if (!desc) return;
    std::string error;
    const auto proto = rm2027::decodeMessage(rm2027::wireDescriptor(*desc), payload.data(), payload.size(), &error);
    google::protobuf::util::JsonPrintOptions options;
    options.always_print_primitive_fields = false;
    std::string json;
    if (proto) {
        const auto status = google::protobuf::util::MessageToJsonString(*proto, &json, options);
        if (!status.ok()) error = status.ToString();
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::lock_guard<std::mutex> lock(recordsMutex_);
    auto &record = records_[topic];
    record.json = json;
    record.error = error;
    record.receivedAt = milliseconds;
    record.receivedTick = monotonicMs();
    ++record.count;
}

QJsonObject Window::messageFields(const std::string &topic) {
    std::string json;
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        const auto it = records_.find(topic);
        if (it == records_.end()) return {};
        json = it->second.json;
    }
    return QJsonDocument::fromJson(QByteArray::fromStdString(json)).object();
}
