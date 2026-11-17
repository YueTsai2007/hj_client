#include "window.h"
#include "window_helpers.h"
#include "map.h"
#include "brand.h"
#include "rm2027/api/wire_format.h"
#include <QIcon>
#include <QStyle>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QFrame *panel(const QString &title, QVBoxLayout *&layout, const char *name = "panel") {
    auto *frame = new GoldFrame;
    frame->setObjectName(name);
    layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(8);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
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
} // namespace

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
    top->addWidget(left, 1); top->addWidget(mapPanel, 3); top->addWidget(right, 1);
    outer->addLayout(top, 3);
    auto *bottom = new QHBoxLayout;
    QVBoxLayout *eventLayout, *metricsLayout, *videoLayout;
    auto *eventPanel = panel(QStringLiteral("战场事件   /   LATEST"), eventLayout);
    events_ = new QListWidget; eventLayout->addWidget(events_);
    auto *metrics = panel(QStringLiteral("战场数据   /   FIELD OVERVIEW"), metricsLayout);
    base_ = label(QStringLiteral("基地血量　等待数据"), "metric");
    economy_ = label(QStringLiteral("剩余经济　等待数据"), "metric");
    damage_ = label(QStringLiteral("累计伤害　等待数据"), "metric");
    outer->insertWidget(1, base_);
    metricsLayout->addWidget(economy_);
    metricsLayout->addWidget(damage_);
    situation_ = new QListWidget;
    situation_->addItem(QStringLiteral("机器人和场地状态：等待数据"));
    metricsLayout->addWidget(situation_, 1);
    auto *videoPanel = panel(QStringLiteral("图传画面   /   VIDEO"), videoLayout);
    videoView_ = label(QStringLiteral("等待图传接入"), "video");
    videoView_->setAlignment(Qt::AlignCenter);
    videoView_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    videoView_->setMinimumSize(250, 140);
    videoStatus_ = label(QStringLiteral("未接收画面"), "caption");
    videoLayout->addWidget(videoView_, 1); videoLayout->addWidget(videoStatus_);
    auto *switchView = new QPushButton(QStringLiteral("放大图传"));
    bar->addWidget(switchView);
    bottom->addWidget(eventPanel, 4);
    bottom->addWidget(metrics, 7);
    bottom->addWidget(videoPanel, 5);
    connect(switchView, &QPushButton::clicked, this, [top, bottom, mapPanel, videoPanel, switchView] {
        const bool enlarge = bottom->indexOf(videoPanel) >= 0;
        top->removeWidget(enlarge ? mapPanel : videoPanel);
        bottom->removeWidget(enlarge ? videoPanel : mapPanel);
        top->insertWidget(1, enlarge ? videoPanel : mapPanel, 3);
        bottom->insertWidget(2, enlarge ? mapPanel : videoPanel, 5);
        switchView->setText(enlarge ? QStringLiteral("返回地图") : QStringLiteral("放大图传"));
    });
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
