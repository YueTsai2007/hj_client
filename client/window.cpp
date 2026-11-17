#include "window.h"
#include "map.h"
#include "brand.h"
#include "rm2027/api/messages.h"
#include "rm2027/api/detail/registry.h"
#include "rm2027/api/wire_format.h"
#include "rm2027/protocol/codec.h"
#include <google/protobuf/util/json_util.h>
#include <QDateTime>
#include <QIcon>
#include <QStyle>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMetaObject>
#include <QPointer>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSplitter>
#include <QScrollBar>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <chrono>

namespace {
qint64 monotonicMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
QLabel *label(const QString &text, const char *object = nullptr) {
    auto *result = new QLabel(text);
    if (object) result->setObjectName(object);
    return result;
}
QFrame *panel(const QString &title, QVBoxLayout *&layout, const char *name = "panel") {
    auto *frame = new GoldFrame;
    frame->setObjectName(name);
    layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(8);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    auto *icon = new QLabel;
    icon->setPixmap(brand::logo(18, icon->devicePixelRatioF()));
    icon->setFixedSize(18, 18);
    head->addWidget(icon);
    const int split = title.indexOf(QStringLiteral("/"));
    head->addWidget(label(split > 0 ? title.left(split).trimmed() : title, "panelTitle"));
    head->addStretch();
    if (split > 0) head->addWidget(label(title.mid(split + 1).trimmed(), "panelTag"));
    layout->addLayout(head);
    auto *rule = new QFrame;
    rule->setObjectName("goldRule");
    rule->setFixedHeight(1);
    layout->addWidget(rule);
    return frame;
}
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
QString robotName(int id) {
    switch (id) {
    case 1: return QStringLiteral("重装");
    case 3: case 4: return QStringLiteral("步兵");
    case 7: return QStringLiteral("哨兵");
    default: return QStringLiteral("机器人");
    }
}
QString teamColor(bool ally, int ownSide) {
    if (ownSide < 0) return QStringLiteral("#a6b7bd");
    const bool red = ally ? ownSide == 0 : ownSide == 1;
    return red ? QStringLiteral("#ff6670") : QStringLiteral("#61c8ff");
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
    if (percentage >= 0) {
        auto *bar = new QProgressBar;
        bar->setRange(0, 1000); bar->setValue(percentage);
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
}

Window::Window(const QString &host, quint16 port, const QString &id, quint16 videoPort) {
    setupUi();
    start(host, port, id, videoPort);
}
Window::~Window() {
    if (timer_) timer_->stop();
    if (client_) client_->stop();
    if (video_) video_->stop();
}
void Window::setupUi() {
    setWindowTitle(QStringLiteral("悍匠 · 战场终端"));
    resize(1600, 960);
    setMinimumSize(1100, 720);
    auto *root = new QWidget;
    root->setObjectName("root");
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(10);
    setWindowIcon(QIcon(brand::logo(128)));
    auto *topBar = new GoldFrame;
    topBar->setObjectName("topBar");
    topBar->setAccent(QColor(brand::kGold));
    auto *header = new QHBoxLayout(topBar);
    header->setContentsMargins(18, 10, 18, 10);
    header->setSpacing(14);
    header->addWidget(new LogoBadge(78));
    auto *identity = new QVBoxLayout;
    identity->setSpacing(2);
    auto *brandName = label(QStringLiteral("悍匠"), "brand");
    QFont brandFont = brandName->font(); brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 10); brandName->setFont(brandFont);
    identity->addStretch();
    identity->addWidget(brandName);
    identity->addWidget(label(QStringLiteral("SHENZHEN TECHNOLOGY UNIVERSITY"), "brandSub"));
    identity->addWidget(label(QStringLiteral("HANJIANG ROBOTEAM  ·  RM2027"), "brandTag"));
    identity->addStretch();
    header->addLayout(identity);
    header->addStretch();
    auto *redCard = new QFrame; redCard->setObjectName("scoreCardRed");
    auto *red = new QVBoxLayout(redCard);
    red->setContentsMargins(22, 6, 22, 6); red->setSpacing(0);
    auto *redCap = label(QStringLiteral("红方比分  RED"), "caption"); redCap->setAlignment(Qt::AlignCenter); red->addWidget(redCap);
    scoreRed_ = label(QStringLiteral("--"), "redScore"); scoreRed_->setAlignment(Qt::AlignCenter); red->addWidget(scoreRed_);
    header->addWidget(redCard);
    auto *clockCard = new QFrame; clockCard->setObjectName("clockCard");
    auto *middle = new QVBoxLayout(clockCard);
    middle->setContentsMargins(26, 6, 26, 6); middle->setSpacing(0);
    round_ = label(QStringLiteral("等待比赛状态"), "round"); round_->setAlignment(Qt::AlignCenter);
    clock_ = label(QStringLiteral("--:--"), "clock"); clock_->setAlignment(Qt::AlignCenter);
    middle->addWidget(round_); middle->addWidget(clock_);
    header->addWidget(clockCard);
    auto *blueCard = new QFrame; blueCard->setObjectName("scoreCardBlue");
    auto *blue = new QVBoxLayout(blueCard);
    blue->setContentsMargins(22, 6, 22, 6); blue->setSpacing(0);
    auto *blueCap = label(QStringLiteral("蓝方比分  BLUE"), "caption"); blueCap->setAlignment(Qt::AlignCenter); blue->addWidget(blueCap);
    scoreBlue_ = label(QStringLiteral("--"), "blueScore"); scoreBlue_->setAlignment(Qt::AlignCenter); blue->addWidget(scoreBlue_);
    header->addWidget(blueCard);
    header->addStretch();
    connection_ = label(QStringLiteral("未连接"), "status"); header->addWidget(connection_, 0, Qt::AlignVCenter);
    outer->addWidget(topBar);
    alert_ = label(QStringLiteral("●  等待连接"), "alert");
    alert_->setWordWrap(true);
    outer->addWidget(alert_);
    auto *bar = new QHBoxLayout;
    bar->addWidget(label(QStringLiteral("战场态势"), "section")); bar->addWidget(label(QStringLiteral("TACTICAL OVERVIEW  /  HANJIANG"), "panelTag"));
    bar->addStretch(); dataStatus_ = label(QStringLiteral("等待数据"), "caption"); bar->addWidget(dataStatus_);
    auto *detailsButton = new QPushButton(QStringLiteral("消息详情"));
    detailsButton->setCursor(Qt::PointingHandCursor);
    connect(detailsButton, &QPushButton::clicked, this, &Window::showDetails);
    bar->addWidget(detailsButton);
    outer->addLayout(bar);
    auto *top = new QHBoxLayout;
    QVBoxLayout *leftLayout, *mapLayout, *rightLayout;
    auto *left = panel(QStringLiteral("我方机器人   /   ALLY UNIT"), leftLayout, "allyPanel");
    ally_ = new QListWidget; ally_->setSelectionMode(QAbstractItemView::NoSelection); ally_->setFocusPolicy(Qt::NoFocus); leftLayout->addWidget(ally_);
    auto *mapPanel = panel(QStringLiteral("战场地图   /   TACTICAL MAP"), mapLayout);
    map_ = new Map; mapLayout->addWidget(map_, 1);
    position_ = label(QStringLiteral("机器人坐标：等待数据"), "caption");
    radar_ = label(QStringLiteral("雷达坐标：等待数据"), "caption");
    position_->setWordWrap(true); radar_->setWordWrap(true);
    mapLayout->addWidget(position_); mapLayout->addWidget(radar_);
    auto *formatLayout = new QHBoxLayout;
    formatLayout->addWidget(label(QStringLiteral("雷达线格式"), "caption"));
    auto *radarFormat = new QComboBox;
    radarFormat->addItem(QStringLiteral("嵌套（nested）"));
    radarFormat->addItem(QStringLiteral("平铺（flat）"));
    radarFormat->setToolTip(QStringLiteral("必须与发送端设置一致；切换后等待新的雷达数据。"));
    connect(radarFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        rm2027::setRadarWireFormat(index == 0 ? rm2027::RadarWireFormat::NestedMessage : rm2027::RadarWireFormat::FlatPackedUint32);
        radarAge_.invalidate();
        radar_->setText(QStringLiteral("雷达格式已切换，等待新数据"));
        std::lock_guard<std::mutex> lock(recordsMutex_);
        records_.erase("RadarInfoToClient");
    });
    formatLayout->addWidget(radarFormat);
    formatLayout->addStretch();
    auto *flip = new QPushButton(QStringLiteral("切换视角"));
    flip->setCursor(Qt::PointingHandCursor);
    connect(flip, &QPushButton::clicked, map_, &Map::flip); formatLayout->addWidget(flip);
    mapLayout->addLayout(formatLayout);
    auto *right = panel(QStringLiteral("敌方机器人   /   ENEMY UNIT"), rightLayout, "enemyPanel");
    enemy_ = new QListWidget; enemy_->setSelectionMode(QAbstractItemView::NoSelection); enemy_->setFocusPolicy(Qt::NoFocus); rightLayout->addWidget(enemy_);
    showHealthPlaceholder(QStringLiteral("等待数据"));
    top->addWidget(left, 1); top->addWidget(mapPanel, 2); top->addWidget(right, 1);
    outer->addLayout(top, 3);
    auto *bottom = new QHBoxLayout;
    QVBoxLayout *eventLayout, *metricsLayout, *videoLayout;
    auto *eventPanel = panel(QStringLiteral("战场事件   /   LATEST"), eventLayout);
    events_ = new QListWidget; eventLayout->addWidget(events_);
    auto *metrics = panel(QStringLiteral("战场数据   /   FIELD OVERVIEW"), metricsLayout);
    base_ = label(QStringLiteral("基地血量　等待数据"), "metric");
    economy_ = label(QStringLiteral("剩余经济　等待数据"), "metric");
    damage_ = label(QStringLiteral("累计伤害　等待数据"), "metric");
    metricsLayout->addWidget(base_); metricsLayout->addWidget(economy_); metricsLayout->addWidget(damage_); metricsLayout->addStretch();
    situation_ = new QListWidget;
    situation_->addItem(QStringLiteral("机器人和场地状态：等待数据"));
    metricsLayout->addWidget(situation_, 1);
    auto *videoPanel = panel(QStringLiteral("图传画面   /   VIDEO"), videoLayout);
    videoView_ = label(QStringLiteral("等待图传接入"), "video"); videoView_->setAlignment(Qt::AlignCenter); videoView_->setMinimumSize(220, 120);
    videoStatus_ = label(QStringLiteral("未接收画面"), "caption");
    videoLayout->addWidget(videoView_, 1); videoLayout->addWidget(videoStatus_);
    bottom->addWidget(eventPanel, 1); bottom->addWidget(metrics, 2); bottom->addWidget(videoPanel, 1);
    outer->addLayout(bottom, 2);
    outer->addWidget(label(QStringLiteral("<span style='color:#e8a33d;font-weight:bold;letter-spacing:2px'>HANJIANG</span>　/　深圳技术大学悍匠战队　　·　　-- 表示字段未提供或数据不可用"), "caption"));
    setCentralWidget(root);
    setStyleSheet(R"(
        QWidget { background: transparent; color: #e8eff4; font-family: 'Inter', 'Microsoft YaHei', 'Noto Sans CJK SC', 'WenQuanYi Zen Hei', sans-serif; font-size: 14px; }
        QMainWindow, QWidget#root { background: qradialgradient(cx:0.06, cy:0, radius:1.1, fx:0.06, fy:0, stop:0 #2a1d0b, stop:0.28 #0d1218, stop:1 #06090d); }
        QDialog, QPlainTextEdit { background: #0b1218; }
        QToolTip { background: #131c26; color: #ffc971; border: 1px solid #e8a33d; padding: 4px 8px; }

        QFrame#topBar { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 rgba(232,163,61,46), stop:0.35 rgba(14,22,33,215), stop:1 rgba(14,22,33,215)); border: 1px solid rgba(232,163,61,95); border-radius: 14px; }
        QFrame#panel, QFrame#allyPanel, QFrame#enemyPanel { background: rgba(14,22,33,205); border: 1px solid rgba(232,163,61,75); border-radius: 12px; }
        QFrame#goldRule { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #e8a33d, stop:0.6 rgba(232,163,61,40), stop:1 rgba(232,163,61,0)); border: 0; max-height: 1px; }
        QFrame#scoreCardRed { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(255,90,103,70), stop:1 rgba(255,90,103,14)); border: 1px solid rgba(255,90,103,150); border-radius: 12px; }
        QFrame#scoreCardBlue { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(62,168,255,70), stop:1 rgba(62,168,255,14)); border: 1px solid rgba(62,168,255,150); border-radius: 12px; }
        QFrame#clockCard { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(232,163,61,60), stop:1 rgba(232,163,61,10)); border: 1px solid #e8a33d; border-radius: 12px; }

        QLabel { background: transparent; border: none; }
        QLabel#brand { font-weight: 800; color: #ffc971; font-size: 30px; }
        QLabel#brandSub { color: #c9d4dc; font-size: 10px; letter-spacing: 1px; }
        QLabel#brandTag { color: #e8a33d; font-size: 10px; font-weight: bold; }
        QLabel#caption { color: #9fb1bf; font-size: 12px; }
        QLabel#round { color: #ffc971; font-size: 12px; }
        QLabel#section { color: #fff4dc; font-weight: bold; font-size: 16px; padding-left: 10px; border-left: 4px solid #e8a33d; }
        QLabel#panelTitle { color: #fff4dc; font-weight: bold; font-size: 15px; }
        QLabel#panelTag { color: #e8a33d; font-size: 10px; font-weight: bold; }
        QLabel#redScore { color: #ff6670; font-weight: 800; font-size: 34px; }
        QLabel#blueScore { color: #61c8ff; font-weight: 800; font-size: 34px; }
        QLabel#clock { color: #fff4dc; font-size: 32px; font-weight: 800; }
        QLabel#status { color: #ffc971; background: rgba(232,163,61,28); border: 1px solid rgba(232,163,61,120); border-radius: 13px; padding: 5px 14px; font-weight: bold; }
        QLabel#status[up="true"] { color: #4ade9a; background: rgba(74,222,154,26); border-color: rgba(74,222,154,130); }
        QLabel#alert { color: #ffd9a0; background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 rgba(232,163,61,60), stop:1 rgba(232,163,61,14)); border: 1px solid rgba(232,163,61,130); border-left: 4px solid #e8a33d; border-radius: 8px; padding: 7px 14px; font-weight: bold; }
        QLabel#alert[ok="true"] { color: #b9f3d6; background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 rgba(74,222,154,50), stop:1 rgba(74,222,154,10)); border-color: rgba(74,222,154,120); border-left-color: #4ade9a; }
        QLabel#metric { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 rgba(232,163,61,36), stop:1 rgba(22,33,48,200)); border: 1px solid rgba(232,163,61,80); border-top: 2px solid #e8a33d; border-radius: 9px; padding: 11px 14px; color: #fff4dc; font-weight: 600; }
        QLabel#robotNum { color: #2a1a04; font-weight: 800; font-size: 16px; border-radius: 8px; background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #ffc971, stop:1 #e8a33d); }
        QLabel#robotName { color: #f2f6f5; font-size: 15px; font-weight: bold; }
        QLabel#robotState, QLabel#robotValue { color: #aebdc7; font-size: 12px; }
        QLabel#video { background: #080d13; border: 1px dashed rgba(232,163,61,130); border-radius: 9px; color: #aebdc7; }

        QListWidget { background: transparent; border: 0; outline: 0; }
        QListWidget::item { color: #d3e0ea; padding: 8px 6px; border-bottom: 1px dashed rgba(232,163,61,60); }
        QFrame#allyPanel QListWidget::item, QFrame#enemyPanel QListWidget::item { background: transparent; border: 0; padding: 0; margin-bottom: 7px; }
        QListWidget::item:selected { background: rgba(232,163,61,40); color: #fff4dc; }

        QPushButton { color: #2a1a04; font-weight: bold; background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffc971, stop:1 #e8a33d); border: 1px solid #ffd9a0; border-radius: 15px; padding: 6px 20px; }
        QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffe0a3, stop:1 #ffc971); border-color: #fff4dc; }
        QPushButton:pressed { background: #c98a2c; }
        QComboBox { color: #fff4dc; background: rgba(22,33,48,230); border: 1px solid rgba(232,163,61,120); border-radius: 8px; padding: 4px 10px; }
        QComboBox:hover { border-color: #ffc971; }
        QComboBox QAbstractItemView { background: #111a25; color: #fff4dc; border: 1px solid #e8a33d; selection-background-color: rgba(232,163,61,90); outline: 0; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: rgba(232,163,61,120); border-radius: 3px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #e8a33d; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }
        QScrollBar::handle:horizontal { background: rgba(232,163,61,120); border-radius: 3px; min-width: 28px; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QSplitter::handle { background: rgba(232,163,61,60); }
    )");
}
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
            self->base_->setText(QStringLiteral("己方基地 %1　/　敌方基地 %2").arg(fieldValue(fields, "baseHealth"), fieldValue(fields, "enemyBaseHealth")));
            self->damage_->setText(QStringLiteral("累计伤害　己方 %1　/　敌方 %2").arg(fieldValue(fields, "totalDamageAlly"), fieldValue(fields, "totalDamageEnemy")));
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
    ally_->clear(); enemy_->clear();
    const int ids[] = {1, 3, 4, 7};
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
        robotCard(list, ids[i % 4], color, status, value, percentage);
    }
}

void Window::showHealthPlaceholder(const QString &status) {
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
